/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Reliable Go-Back-N Transport Layer.
 */
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>
#include "net.h"
#include "../core.h"

//#define CONFIG_DEBUG_GBN

#ifdef CONFIG_DEBUG_GBN
#define gbn_debug(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__)
#else
#define gbn_debug(fmt, ...)  do { } while (0)
#endif

#define gbn_info(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__)

/*
 * The maximum number of unack'ed outgoing messages.
 * Organized as a ring.
 */
#define NR_BUFFER_INFO_SLOTS		(256)
#define GBN_RETRANS_TIMEOUT_US		(2000)
#define GBN_TIMEOUT_CHECK_INTERVAL_MS	(5)

static int polling_thread_created = 0;
static pthread_t polling_thread;

/*
 * This is a global variable controlling the buffer mgmt behavior.
 * If underlying raw net layer supports zerocopy receive, we will NOT
 * need to manage buffers, simply saving the pointer given by raw net.
 * Otherwise, we need to create buffer and a lot copies are involved.
 */
static bool use_zerocopy;

#define BUFFER_INFO_ALLOCATED	(0x1)
struct buffer_info {
	unsigned int flags;
	void *buf;
	size_t buf_size;
	struct list_head list;
};

static inline void set_buffer_info_allocated(struct buffer_info *info)
{
	info->flags |= BUFFER_INFO_ALLOCATED;
}

static inline void clear_buffer_info_allocated(struct buffer_info *info)
{
	info->flags &= ~((unsigned int)BUFFER_INFO_ALLOCATED);
}

static inline bool buffer_info_allocated(struct buffer_info *info)
{
	if (info->flags & BUFFER_INFO_ALLOCATED)
		return true;
	return false;
}

struct session_gbn {
	/*
	 * The ring of unack'ed outgoing packets:
	 *
	 * |----------------|-------------------|-------------------|
	 *                 ^  (unack'ed slots) ^
	 *   (free slots) tail                head   (free slots)
	 *                 ^                   ^
	 *            (seqnum_last)       (seqnum_cur)
	 *
	 * Sequence numeber starts from 0, and increase monotonically.
	 * seqnum_cur is the head, seqnum_last is the tail.
	 * seqnum_cur is the seqnum of most recently sentout packet
	 * seqnum_last is the seqnum of most recently acked packet
	 */
	struct buffer_info		unack_buffer_info_ring[NR_BUFFER_INFO_SLOTS];
	atomic_int			seqnum_cur;
	atomic_int			seqnum_last;
	
	/* Timer */
	timer_t				rt_timer;
	struct sigevent			timeout_event;

	/*
	 * For the incoming data, we do it differently with the above approach.
	 * We use a linked-list to link all incoming data.
	 * This ring below is used to accelerate malloc/free, like kmem_cache.
	 * This data_list is the real placeholder.
	 *
	 * The reason for this extra list is that mutlithread dequeue of above
	 * ring-buffer like design is hard to implement. Easier to have a list.
	 */
	struct list_head		data_list;
	pthread_spinlock_t		data_lock;
	int				nr_data;
	atomic_int			seqnum_expect;
	bool				ack_enable;

	struct buffer_info		data_buffer_info_ring[NR_BUFFER_INFO_SLOTS];
	atomic_int			data_buffer_info_HEAD;

	/* Stats */
	unsigned long			nr_rx_ack;
	unsigned long			nr_rx_nack;
	unsigned long			nr_rx_data;
};

static __always_inline struct buffer_info *
index_to_unack_buffer_info(struct session_gbn *ses, unsigned int index)
{
	struct buffer_info *info;

	index %= NR_BUFFER_INFO_SLOTS;
	info = &ses->unack_buffer_info_ring[index];
	return info;
}

static __always_inline struct buffer_info *
index_to_data_buffer_info(struct session_gbn *ses, unsigned int index)
{
	struct buffer_info *info;

	index %= NR_BUFFER_INFO_SLOTS;
	info = &ses->data_buffer_info_ring[index];
	return info;
}

static __always_inline void
disable_timeout(struct session_gbn *ses)
{
	struct itimerspec timeout;

	timeout.it_interval.tv_nsec = 0;
	timeout.it_interval.tv_sec = 0;
	timeout.it_value.tv_nsec = 0;
	timeout.it_value.tv_sec = 0;

	timer_settime(ses->rt_timer, 0, &timeout, NULL);
}

static __always_inline void
set_next_timeout(struct session_gbn *ses)
{
	struct itimerspec timeout;

	timeout.it_interval.tv_nsec = 0;
	timeout.it_interval.tv_sec = 0;
	timeout.it_value.tv_nsec = GBN_RETRANS_TIMEOUT_US * 1000;
	timeout.it_value.tv_sec = 0;

	// Set flag to 0, timeout is relative time to current time
	timer_settime(ses->rt_timer, 0, &timeout, NULL);
}

/*
 * At x86, simple aligned operations are atomic.
 * Thus atomic_load is simply a normal read.
 */
static inline int nr_unack_buffer(struct session_gbn *ses)
{
	return atomic_load(&ses->seqnum_cur) - atomic_load(&ses->seqnum_last);
}

/*
 * This function allocate a new unack buffer slot in a SMP-safe way.
 * It may wait a bit if the ring buffer is full, i.e., a lot unack'ed packets.
 * We return the new slot and @seqnum.
 */
static __always_inline struct buffer_info *
alloc_unack_buffer_info(struct session_gbn *ses, int *seqnum)
{
#define GBN_ALLOC_UNACK_TIMEOUT_S	(2)
	struct buffer_info *info;
	int index;

	/*
	 * XXX:
	 * even if alloc fails, sequence num is still increased
	 * this will result in missing sequence
	 */
	index = atomic_fetch_add(&ses->seqnum_cur, 1);

	/* index = seq - 1 */
	info = index_to_unack_buffer_info(ses, index);

	if (unlikely(buffer_info_allocated(info))) {
		/*
		 * Okay, the buffer slot is still in use,
		 * which means it has not been ACK'ed.
		 * We wait here with a max timeout detection.
		 */
		struct timespec s, e;
		clock_gettime(CLOCK_MONOTONIC, &s);
		while (1) {
			if (likely(!buffer_info_allocated(info)))
				break;
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) >= GBN_ALLOC_UNACK_TIMEOUT_S) {
				gbn_info("alloc unack buffer timeout (%d s). "
					   "Probably there is no ACK from other side\n",
					   GBN_ALLOC_UNACK_TIMEOUT_S);
				return NULL;
			}
		}
	}

	/*
	 * Prepare the new slot
	 * Return the new seqnum
	 * seq = index + 1, as seq# starts from 1, e.g. seq 1->index 0
	 */
	set_buffer_info_allocated(info);
	*seqnum = index + 1;
	barrier();
	return info;
}

/* Alternative to normal malloc(buffer_info) */
static __always_inline struct buffer_info *
alloc_data_buffer_info(struct session_gbn *ses)
{
#define GBN_ALLOC_DATA_TIMEOUT_S	(2)
	struct buffer_info *info;
	int index;

	index = atomic_fetch_add(&ses->data_buffer_info_HEAD, 1);
	info = index_to_data_buffer_info(ses, index);

	if (unlikely(buffer_info_allocated(info))) {
		/*
		 * Ring is full, we step into an on-use one.
		 * Wait until it's free.
		 */
		struct timespec s, e;
		clock_gettime(CLOCK_MONOTONIC, &s);
		while (1) {
			if (likely(!buffer_info_allocated(info)))
				break;
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) >= GBN_ALLOC_DATA_TIMEOUT_S) {
				gbn_info("alloc data buffer timeout (%d s). ",
					GBN_ALLOC_DATA_TIMEOUT_S);
				return NULL;
			}
		}
	}

	/* Prepare the new slot */
	set_buffer_info_allocated(info);
	barrier();
	return info;
}

/* Alternative to normal free(info) */
static __always_inline void
free_data_buffer_info(struct buffer_info *info)
{
	barrier();
	clear_buffer_info_allocated(info);
}

/*
 * The caller needs to make sure it's legitimate to free it,
 * i.e., it must sit in or after the seqnum_last.
 */
static __always_inline void
free_unack_buffer_info(struct buffer_info *info)
{
	barrier();
	clear_buffer_info_allocated(info);
}

static __always_inline void
enqueue_data_buffer_info_head(struct session_gbn *ses, struct buffer_info *info)
{
	pthread_spin_lock(&ses->data_lock);
	list_add(&info->list, &ses->data_list);
	ses->nr_data++;
	pthread_spin_unlock(&ses->data_lock);
}

static __always_inline void
enqueue_data_buffer_info_tail(struct session_gbn *ses, struct buffer_info *info)
{
	pthread_spin_lock(&ses->data_lock);
	list_add_tail(&info->list, &ses->data_list);
	ses->nr_data++;
	pthread_spin_unlock(&ses->data_lock);
}

static __always_inline struct buffer_info *
dequeue_data_buffer_info_head(struct session_gbn *ses)
{
	struct buffer_info *info = NULL;

	pthread_spin_lock(&ses->data_lock);
	if (!list_empty(&ses->data_list)) {
		info = list_first_entry(&ses->data_list,
					struct buffer_info, list);
		list_del(&info->list);
		ses->nr_data--;
	}
	pthread_spin_unlock(&ses->data_lock);
	return info;
}

static bool
handle_ack_nack_dequeue(struct gbn_header *hdr, struct session_gbn *ses_gbn,
		        bool is_ack)
{
	unsigned int i;
	unsigned int seq, seqnum_last;
	struct buffer_info *info;

	/* Update stat */
	if (hdr->type == GBN_PKT_ACK)
		ses_gbn->nr_rx_ack++;
	else if (hdr->type == GBN_PKT_NACK)
		ses_gbn->nr_rx_nack++;

	seqnum_last = atomic_load(&ses_gbn->seqnum_last);
	seq = hdr->seqnum;
	info = index_to_unack_buffer_info(ses_gbn, seq - 1);

	if (likely(seq > seqnum_last)) {
		/*
		 * ACK valid, normal case is seq = seqnum_last + 1 when there is no packet loss
		 * Free all the unack buffer with seqnum no greater than received seq
		 * and move expected seqnum forward.
		 *
		 * Both ACK and NACK packet can be here.
		 * For NACK:
		 */
		for (i = seqnum_last; i < seq; i++) {
			info = index_to_unack_buffer_info(ses_gbn, i);
			free_unack_buffer_info(info);
		}
		atomic_store(&ses_gbn->seqnum_last, seq);

		/*
		 * If unack buffer is empty after dequeue, then we do not need to take
		 * care of timeout anymore. If it's ack packet, reset timeout.
		 * If it's nack packet, reset after finish retransmission
	 	 */
		if (!nr_unack_buffer(ses_gbn))
			disable_timeout(ses_gbn);
		else if (hdr->type == GBN_PKT_ACK)
			set_next_timeout(ses_gbn);

		return true;
	} else if (seq == seqnum_last) {
		/*
		 * ACK valid, received seqnum is the same as seqnum last, return directly
		 * This usually happens when nack is received,
		 * since we response nack (last acked seqnum) from the receiver side
		 */
		return true;
	} else {
		/*
		 * ACK invalid, received seqnum is less than seqnum last
		 * This case should not happen
		 */
		gbn_info("WARN. Recevied Seq: %u. less than Last Acked Seq: %u\n",
			  seq, seqnum_last);
		return false;
	}
}

static void
retrans_unack_buffer_info(struct session_net *ses_net, struct session_gbn *ses_gbn)
{
	struct buffer_info *info;
	unsigned int i;
	unsigned int retrans_start, retrans_end;
	int ret;

	retrans_start = atomic_load(&ses_gbn->seqnum_last);
	retrans_end = atomic_load(&ses_gbn->seqnum_cur);
	gbn_debug("retrans %d to %d, session %d\n", retrans_start, retrans_end,
		  ses_net->session_id);
	for (i = retrans_start; i < retrans_end; i++) {
		/*
		 * retrans seq#: (seqnum_last, seqnum_cur]
		 *     ->index#: [seqnum_last, seqnum_cur)
		 */
		info = index_to_unack_buffer_info(ses_gbn, i);

		ret = raw_net_send(ses_net, info->buf, info->buf_size, NULL);
		if (ret < 0) {
			gbn_info("net_send error %d\n", ret);
			break;
		}
	}

	/*
	 * if unack buffer is not empty after dequeue,
	 * reset timeout after retrans.
	 * This case should always happen
	 */
	if (likely(retrans_end - retrans_start > 0))
		set_next_timeout(ses_gbn);
}

static void handle_ack_packet(struct session_net *ses_net, void *packet)
{
	struct gbn_header *hdr;
	struct session_gbn *ses_gbn;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_info("ERROR: corrupted ses_gbn %p\n", ses_net);
		return;
	}

	hdr = to_gbn_header(packet);

	handle_ack_nack_dequeue(hdr, ses_gbn, true);
}

static void handle_nack_packet(struct session_net *ses_net, void *packet)
{
	struct gbn_header *hdr;
	struct session_gbn *ses_gbn;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_info("ERROR: corrupted ses_gbn %p\n", ses_net);
		return;
	}

	hdr = to_gbn_header(packet);

	/*
	 * If received packet is valid, retransmit
	 */
	if (handle_ack_nack_dequeue(hdr, ses_gbn, false))
		retrans_unack_buffer_info(ses_net, ses_gbn);
}

/*
 * Called by polling thread.
 * Push new data into cached ring buffer list.
 */
static void handle_data_packet(struct session_net *ses_net,
			       void *packet, size_t buf_size)
{
	struct gbn_header *hdr;
	struct session_gbn *ses_gbn;
	struct buffer_info *info;
	unsigned int seq;
	int ret;

	struct eth_ack_packet {
		char	placeholder[GBN_HEADER_OFFSET];
		struct	gbn_header ack_header;
	} __packed ack;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_info("ERROR: corrupted ses_gbn %p\n", ses_net);
		return;
	}

	/*
	 * Mgmt traffic is unreliable
	 * The sender skipped unack buffer,
	 * and we, as the receiver, do not need to send ACK.
	 * Simply attaching this buffer into the list is enough.
	 */
	if (test_management_session(ses_net)) {
		ses_gbn->nr_rx_data++;

		info = alloc_data_buffer_info(ses_gbn);
		if (!info)
			return;

		if (likely(use_zerocopy))
			info->buf = packet;
		else
			memcpy(info->buf, packet, buf_size);

		info->buf_size = buf_size;
		enqueue_data_buffer_info_tail(ses_gbn, info);
		return;
	}

	hdr = to_gbn_header(packet);
	seq = hdr->seqnum;
	/* ACK packet has swapped session id as DATA packet */
	set_gbn_src_dst_session(&ack.ack_header,
				get_gbn_dst_session(hdr),
				get_gbn_src_session(hdr));

	if (likely(seq == atomic_load(&ses_gbn->seqnum_expect))) {
		/*
		 * seqnum is valid and as expected,
		 * send back ACK, enable response, and increase expected seqnum
		 */
		ses_gbn->nr_rx_data++;

		info = alloc_data_buffer_info(ses_gbn);
		if (!info)
			return;

		/*
		 * If there is no zerocopy, we need to copy
		 * the conent into the info->buf..
		 */
		if (likely(use_zerocopy))
			info->buf = packet;
		else
			memcpy(info->buf, packet, buf_size);

		info->buf_size = buf_size;

		ses_gbn->ack_enable = true;

		ack.ack_header.type = GBN_PKT_ACK;
		ack.ack_header.seqnum = atomic_fetch_add(&ses_gbn->seqnum_expect, 1);

		/*
		 * Enqueue this data into the list before we send out
		 * the ACK. Once enqueued, this packet is visiable to user.
		 * This could have some perf benefit.
		 */
		enqueue_data_buffer_info_tail(ses_gbn, info);

		ret = raw_net_send(ses_net, &ack, sizeof(ack), NULL);
		if (ret < 0) {
			gbn_info("net_send error %d\n", ret);
			return;
		}
	} else if (ses_gbn->ack_enable) {
		if (seq > atomic_load(&ses_gbn->seqnum_expect)) {
			/*
			 * seqnum invalid, if response is enabled,
			 * send back NACK and disable further response
			 */
			ses_gbn->ack_enable = false;
			ack.ack_header.type = GBN_PKT_NACK;
			ack.ack_header.seqnum = atomic_load(&ses_gbn->seqnum_expect) - 1;

			ret = raw_net_send(ses_net, &ack, sizeof(ack), NULL);
			if (ret < 0) {
				gbn_info("net_send error %d\n", ret);
				return;
			}
		} else if (seq < atomic_load(&ses_gbn->seqnum_expect)) {
			/*
			 * seqnum valid, but not as expected.
			 * if response is enabled, send back ACK
			 */
			ack.ack_header.type = GBN_PKT_ACK;
			ack.ack_header.seqnum = atomic_load(&ses_gbn->seqnum_expect) - 1;

			ret = raw_net_send(ses_net, &ack, sizeof(ack), NULL);
			if (ret < 0) {
				gbn_info("net_send error %d\n", ret);
				return;
			}
		}
	}
}

/*
 * This is Go-Back-N transport layer's polling thread.
 * It invokes lower level raw network interface to receive new packets.
 * It then inspects the packet contents, either clear unack'ed buffers,
 * or cache the data packets and generate ACK.
 */
static void *gbn_poll_func(void *_unused)
{
	struct session_net *ses_net;
	struct gbn_header *gbn_hdr;
	struct udp_hdr *udp_hdr;
	struct ipv4_hdr *ipv4_hdr;
	unsigned int udp_port, remote_ip, dst_sesid;
	void *recv_buf;
	size_t buf_size, max_buf_size;
	int ret;
	char packet_dump_str[256];

	/*
	 * If there is no zerocopy,
	 * we need to Bring Our Own Buffers.
	 */
	if (!use_zerocopy) {
		max_buf_size = sysctl_link_mtu;
		recv_buf = malloc(max_buf_size);
		if (!recv_buf) {
			printf("%s(): Fail to alloc recv_buf\n", __func__);
			return NULL;
		}
	}

	while (1) {
		if (likely(use_zerocopy)) {
			ret = raw_net_receive_zerocopy(&recv_buf, &buf_size);
			if (unlikely(ret < 0)) {
				gbn_info("zerocopy recv error %d\n", ret);
				goto out;
			} else if (ret == 0)
				continue;
		} else {
			buf_size = raw_net_receive(recv_buf, max_buf_size);
			if (unlikely(buf_size < 0)) {
				gbn_info("receive error %ld\n", buf_size);
				goto out;
			} else if (buf_size == 0)
				continue;
		}

		gbn_hdr = to_gbn_header(recv_buf);
		ipv4_hdr = to_ipv4_header(recv_buf);
		udp_hdr = to_udp_header(recv_buf);

		dst_sesid = get_gbn_dst_session(gbn_hdr);
		remote_ip = ntohl(ipv4_hdr->src_ip);
		udp_port = ntohs(udp_hdr->src_port);

		/*
		 * Try to locate the net session. There are 3 cases:
		 * 1) local mgmt session, if dst session id is 0.
		 * 2) the matched session, if both IP+ID match.
		 * 3) NULL, if none of the above options fulfill
		 */
		ses_net = find_net_session(remote_ip, udp_port, dst_sesid);
		if (unlikely(!ses_net)) {
			char str[INET_ADDRSTRLEN];
			struct in_addr in_addr;
			in_addr.s_addr = ipv4_hdr->src_ip;
			inet_ntop(AF_INET, &in_addr, str, sizeof(str));

			gbn_debug("Session not found! src_ip: %s src_sesid: %u dst_sesid: %u\n",
				str, get_gbn_src_session(gbn_hdr), dst_sesid);
			continue;
		}

		dump_packet_headers(recv_buf, packet_dump_str);
		gbn_debug("new pkt: %s ses: %u->%u type: %s\n",
			packet_dump_str,
			get_gbn_src_session(gbn_hdr), dst_sesid,
			gbn_pkt_type_str(gbn_hdr->type));

		switch (gbn_hdr->type) {
		case GBN_PKT_ACK:
			handle_ack_packet(ses_net, recv_buf);
			break;
		case GBN_PKT_NACK:
			handle_nack_packet(ses_net, recv_buf);
			break;
		case GBN_PKT_DATA:
			handle_data_packet(ses_net, recv_buf, buf_size);
			break;
		default:
			gbn_info("WARN: Unknown GBN packet type: %d\n",
				gbn_hdr->type);
			break;
		};
	}
out:
	return NULL;
}

/*
 * Fill these fields
 * - src session id
 * - dst session id
 */
static __always_inline void
prepare_gbn_headers(struct gbn_header *hdr, struct session_net *net)
{
	unsigned int src_sesid, dst_sesid;

	src_sesid = get_local_session_id(net);
	dst_sesid = get_remote_session_id(net);

	set_gbn_src_dst_session(hdr, src_sesid, dst_sesid);
}

/*
 * This function is multithread safe both within and across sessions.
 * But if a session is used by multiple threads, the caller itself
 * need to ensure the ordering among threads.
 */
static inline int gbn_send_one(struct session_net *net,
			       void *buf, size_t buf_size, void *route)
{
	struct buffer_info *info;
	struct session_gbn *ses;
	int seqnum;
	bool unacked_buffer_empty;
	struct gbn_header *hdr;

	hdr = to_gbn_header(buf);

	if (route) {
		struct gbn_header *user_hdr;
		user_hdr = to_gbn_header(route);
		memcpy(hdr, user_hdr, sizeof(*hdr));
	} else {
		hdr->type = GBN_PKT_DATA;
		prepare_gbn_headers(hdr, net);
	}

	/*
	 * Any traffic targeting and originate from mgmt session
	 * is connectionless and unreliable. Thus skip everything.
	 */
	if (test_management_session(net))
		goto send;

	ses = (struct session_gbn *)net->transport_private;
	BUG_ON(!ses);

	/*
	 * We check if unacked buffer is empty before grabbing a slot,
	 * since grabing a slot will increase seqnum_cur and the 
	 * buffer can never be empty if the seqnum_cur is increased
	 */
	unacked_buffer_empty = !nr_unack_buffer(ses);

	/* Try to alloc a new unack buffer and seqnum */
	info = alloc_unack_buffer_info(ses, &seqnum);
	if (!info)
		return -ENOMEM;

	/* Save into the pkt */
	hdr->seqnum = seqnum;

	info->buf = buf;
	info->buf_size = buf_size;

	/*
	 * We do not reset timeout for every packet sent out.
	 * Timeout is only (re)set when the unack buffer is originally empty
	 */
	if (unacked_buffer_empty) {
		set_next_timeout(ses);
	}

send:
	return raw_net_send(net, buf, buf_size, route);
}

/*
 * GBN receive a packet, i.e., dequeue a packet from the data buffer array.
 * Pakets in that array have been ack'ed and are ready for grab.
 *
 * This is a blocking call, thus we set the timeout to be super large.
 *
 * Since a session is only supposed be used by one thread, we do
 * not need to worry about the case where multiple threads trying
 * to send/receive using the same session. For that case, some sort
 * of extra info is needed to ensure ordering.
 */
static inline int gbn_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
#define GBN_RECEIVE_TIMEOUT_S		(3600)
	struct session_gbn *ses;
	struct buffer_info *info;
	size_t ret;

	ses = (struct session_gbn *)net->transport_private;
	BUG_ON(!ses);

	/*
	 * To avoid spinning the lock, we just do a light
	 * dequeue check, and then spinning on the counter.
	 */
retry:
	info = dequeue_data_buffer_info_head(ses);
	if (!info) {
		struct timespec s, e;

		clock_gettime(CLOCK_MONOTONIC, &s);
		while (!ses->nr_data) {
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) > GBN_RECEIVE_TIMEOUT_S) {
				printf("gbn receive: timeout\n");
				return -ETIMEDOUT;
			}
		}
		goto retry;
	}

	/* Put the data back to the head if too small */
	if (unlikely(info->buf_size > buf_size)) {
		printf("gbn: User provided recv buf is too small. (%zu %zu)\n",
			info->buf_size, buf_size);
		enqueue_data_buffer_info_head(ses, info);
		return -ENOMEM;
	}

	/*
	 * Whether zerocopy is enabled or not, we need to do this copy.
	 * Of course, this copy can be reduced if users are willing to
	 * give up this interface and use a RPC-like way.
	 */
	memcpy(buf, info->buf, info->buf_size);

	ret = info->buf_size;
	free_data_buffer_info(info);
	return ret;
}

/*
 * Non-blocking version, if the list is empty, just return.
 */
static inline int gbn_receive_one_nb(struct session_net *net,
				     void *buf, size_t buf_size)
{
	return -ENOSYS;
}

void gbn_timeout_handler(union sigval val)
{
	struct session_net *ses_net;
	struct session_gbn *ses_gbn;

	ses_net = (struct session_net *)val.sival_ptr;
	ses_gbn = (struct session_gbn *)ses_net->transport_private;

	gbn_debug("Session %d timeout\n", ses_net->session_id);
	retrans_unack_buffer_info(ses_net, ses_gbn);
}

static int init_session_gbn(struct session_net* net, struct session_gbn *ses)
{
	int i;
	struct buffer_info *info;
	int ret;

	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_unack_buffer_info(ses, i);
		memset(info, 0, sizeof(*info));
		INIT_LIST_HEAD(&info->list);
	}
	atomic_init(&ses->seqnum_cur, 0);
	atomic_init(&ses->seqnum_last, 0);

	/*
	 * Create timer for this gbn session
	 * Store net session in the sigevent struct
	 */
	ses->timeout_event.sigev_notify = SIGEV_THREAD;
	ses->timeout_event.sigev_value.sival_ptr = (void *)net;
	ses->timeout_event.sigev_notify_function = gbn_timeout_handler;
	ret = timer_create(CLOCK_MONOTONIC, &ses->timeout_event, &ses->rt_timer);
	if (ret < 0) {
		ret = -errno;
		printf("gbn: Failed to create timer\n");
		goto out;
	}

	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_data_buffer_info(ses, i);
		memset(info, 0, sizeof(*info));
		INIT_LIST_HEAD(&info->list);

		/*
		 * If there is no zerocopy, we need to
		 * allocate the data buffers. The polling
		 * thread will copy the packet into this buffer
		 * instead of saving the pointer.
		 */
		if (!use_zerocopy) {
			info->buf = malloc(sysctl_link_mtu);
			if (!info->buf) {
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	atomic_init(&ses->data_buffer_info_HEAD, 0);
	atomic_init(&ses->seqnum_expect, 1);

	INIT_LIST_HEAD(&ses->data_list);
	pthread_spin_init(&ses->data_lock, PTHREAD_PROCESS_PRIVATE);
	ses->nr_data = 0;

	ret = 0;
out:
	return ret;
}

static int
gbn_open_session(struct session_net *ses_net, struct endpoint_info *local_ei,
		 struct endpoint_info *remote_ei)
{
	struct session_gbn *ses_gbn;
	int ret;

	ses_gbn = malloc(sizeof(struct session_gbn));
	if (!ses_gbn)
		return -ENOMEM;
	ses_net->transport_private = ses_gbn;

	ret = init_session_gbn(ses_net, ses_gbn);
	if (ret) {
		free(ses_gbn);
		ses_net->transport_private = NULL;
		return ret;
	}

	return 0;
}

static int gbn_close_session(struct session_net *ses_net)
{
	struct session_gbn *ses_gbn;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (ses_gbn) {
		timer_delete(ses_gbn->rt_timer);
		free(ses_gbn);
	}
	return 0;
}

#if 0
/*
 * This is go-back-N transport layer's timeout watcher thread.
 * It wakes up every 0.5ms to check if a timeout event happends.
 * If timeout, it will start a retransmission.
 * XXX
 * We maynot want to use a signal-like approach, since it involve with kernel
 *
 * TODO the timeout is simply not working
 */
static void *gbn_timeout_watcher_func(void *_unused)
{
	/*
	 * XXX Yizhou
	 * This thread will not work as expected in multi session case.
	 * We need to use per-session timers now.
	 */
	struct session_net *ses_net;
	struct session_gbn *ses_gbn;
	struct timespec cur_time;

	ses_net = (struct session_net *)arg;
	ses_gbn = (struct session_gbn *)ses_net->transport_private;

	while (1)
	{
		clock_gettime(CLOCK_MONOTONIC, &cur_time);

		/* if timeout, start retransmission */
		if (time_greater_than(&cur_time, &ses_gbn->next_timeout)) {
			printf("cur: %ld.%ld, timeout: %ld.%ld\n",
			       cur_time.tv_sec, cur_time.tv_nsec,
			       ses_gbn->next_timeout.tv_sec,
			       ses_gbn->next_timeout.tv_nsec);
			retrans_unack_buffer_info(ses_net, ses_gbn);
		}

		usleep(1000 * GBN_TIMEOUT_CHECK_INTERVAL_MS);
	}
	return NULL;
}
#endif

/*
 * For now, we only create one global polling thread per machine.
 * If this becomes a bottleneck, we may well change this.
 */
static int create_polling_thread(void)
{
	int ret;

	if (polling_thread_created)
		return 0;

	ret = pthread_create(&polling_thread, NULL, gbn_poll_func, NULL);
	if (ret) {
		printf("GBN: Fail to create the polling thread\n");
		return ret;
	}
	printf("Created the go-back-N reliable net polling thread\n");
	return 0;
}

static int gbn_init_once(struct endpoint_info *local_ei)
{
	int ret;

	/* One global polling thread */
	ret = create_polling_thread();
	if (ret) {
		printf("Fail to create GBN polling thread\n");
		return ret;
	}

	/*
	 * Zerocopy or not affects our buffer mgmt method
	 * Do this early on, and we will check at various cases.
	 */
	if (raw_net_ops->receive_one_zerocopy)
		use_zerocopy = true;
	else
		use_zerocopy = false;

	return 0;
}

static void gbn_exit(void)
{
	return;
}

struct transport_net_ops transport_gbn_ops = {
	.name			= "transport_rel_gbn",

	.init_once		= gbn_init_once,
	.exit			= gbn_exit,

	.open_session		= gbn_open_session,
	.close_session		= gbn_close_session,

	.send_one		= gbn_send_one,
	.receive_one		= gbn_receive_one,
	.receive_one_nb		= gbn_receive_one_nb,
};
