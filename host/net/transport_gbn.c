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

#if 0
#define CONFIG_GBN_DEBUG
#endif

bool stop_gbn_poll_thread = false;

/*
 * This option controls whether host side GBN stack will maintain
 * a per-session timer. If enabled, every packet TX/RX will involve
 * kernel timer operation. We found the overhead is non-trivial.
 *
 * We can disable timer thanks to our unique RPC like semantic:
 * a caller using our API will first send msg to remote, and then
 * wait for reply. Thus the waiting part can be used as timeout detection.
 */
#if 0
#define CONFIG_GBN_ENABLE_TIMER
#endif

#ifdef CONFIG_GBN_DEBUG
# define gbn_debug(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__)
#else
# define gbn_debug(fmt, ...)  do { } while (0)
#endif

#define gbn_info(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__)

/*
 * The maximum number of unack'ed outgoing messages.
 * Organized as a ring.
 */
#define NR_BUFFER_INFO_SLOTS		(256)
#define GBN_RETRANS_TIMEOUT_US		(4000)
#define GBN_TIMEOUT_CHECK_INTERVAL_MS	(5)
#define GBN_RECEIVE_MAX_TIMEOUT_S	(10)

static int polling_thread_created = 0;
static pthread_t polling_thread;

/*
 * This is a global variable controlling the buffer mgmt behavior.
 * If underlying raw net layer supports zerocopy receive, we will NOT
 * need to manage buffers, simply saving the pointer given by raw net.
 * Otherwise, we need to create buffer and a lot copies are involved.
 */
static bool raw_net_has_zerocopy __read_mostly;

#define BUFFER_INFO_ALLOCATED	(0x1u)
#define BUFFER_INFO_USABLE	(0x2u)
struct buffer_info {
	void 		*buf;
	unsigned int	buf_size;
	unsigned int	flags;
} __packed __aligned(8);

static inline void __set_buffer_info_usable(struct buffer_info *info)
{
	info->flags |= BUFFER_INFO_USABLE;
}

/*
 * READ_ONCE is necessary, otherwise gcc will create some issues
 * for wait_data_buffer_usable. Basically it thinks this is single thread
 * code and get rid of the repeat access. We are using it to synchronize
 * between the polling thread and user receive_one thread.
 */
static inline bool __test_buffer_info_usable(struct buffer_info *info)
{
	if (READ_ONCE(info->flags) & BUFFER_INFO_USABLE)
		return true;
	return false;
}

static inline void __set_buffer_info_allocated(struct buffer_info *info)
{
	info->flags |= BUFFER_INFO_ALLOCATED;
}

static inline bool __test_buffer_info_allocated(struct buffer_info *info)
{
	if (READ_ONCE(info->flags) & BUFFER_INFO_ALLOCATED)
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
	
	atomic_int			seqnum_expect;
	bool				ack_enable;

	struct msg_buf			*mb_ack_reply;

	/*
	 * A session is only supposed to be used by one user thread.
	 * Thus we maintaina a single-producer-single-consumer model.
	 * Thus two normal unsigned int are good enough.
	 */
	struct buffer_info		data_buffer_info_ring[NR_BUFFER_INFO_SLOTS];
	unsigned int			data_buffer_info_HEAD;
	unsigned int			data_buffer_info_TAIL;

	/* Stats */
	unsigned long			nr_rx_ack;
	unsigned long			nr_rx_nack;
	unsigned long			nr_rx_data;
} __aligned(64);

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

#ifdef CONFIG_GBN_ENABLE_TIMER
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

	/* Set flag to 0, timeout is relative time to current time */
	timer_settime(ses->rt_timer, 0, &timeout, NULL);
}
#else
static __always_inline void disable_timeout(struct session_gbn *ses) { }
static __always_inline void set_next_timeout(struct session_gbn *ses) { }
#endif /* CONFIG_GBN_ENABLE_TIMER */

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

	if (unlikely(__test_buffer_info_allocated(info))) {
		/*
		 * Okay, the buffer slot is still in use,
		 * which means it has not been ACK'ed.
		 * We wait here with a max timeout detection.
		 */
		struct timespec s, e;
		clock_gettime(CLOCK_MONOTONIC, &s);
		while (1) {
			if (likely(!__test_buffer_info_allocated(info)))
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
	__set_buffer_info_allocated(info);
	*seqnum = index + 1;
	barrier();
	return info;
}

static __always_inline unsigned int
nr_data_buffer_info(struct session_gbn *ses)
{
	return ses->data_buffer_info_HEAD - ses->data_buffer_info_TAIL;
}

static __always_inline struct buffer_info *
alloc_data_buffer_info(struct session_gbn *ses)
{
#define GBN_ALLOC_DATA_TIMEOUT_S	(5)
	struct buffer_info *info;
	int index;

	index = ses->data_buffer_info_HEAD++;
	info = index_to_data_buffer_info(ses, index);

	if (unlikely(__test_buffer_info_allocated(info))) {
		struct timespec s, e;
		clock_gettime(CLOCK_MONOTONIC, &s);
		while (1) {
			if (likely(!__test_buffer_info_allocated(info)))
				break;
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) >= GBN_ALLOC_DATA_TIMEOUT_S) {
				gbn_info("alloc data buffer timeout (%d s). "
					 "maybe some one forgot to grab the packets?\n",
					GBN_ALLOC_DATA_TIMEOUT_S);
				return NULL;
			}
		}
	}

	/* Prepare the new slot */
	__set_buffer_info_allocated(info);
	return info;
}

static __always_inline void
free_data_buffer_info(struct buffer_info *info)
{
	info->flags = 0;

	/*
	 * Make sure no other updates reordered
	 * by the compiler.
	 */
	barrier();
}

/*
 * The caller needs to make sure it's legitimate to free it,
 * i.e., it must sit in or after the seqnum_last.
 */
static __always_inline void
free_unack_buffer_info(struct buffer_info *info)
{
	info->flags = 0;
	barrier();
}

void dump_gbn_session(struct session_net *net, bool dump_rx_ring)
{
	struct session_gbn *ses;

	ses = (struct session_gbn *)net->transport_private;
	printf("Dump gbn session: \n"
	       "  [RX]\n"
	       "    ring_HEAD: %u\n"
	       "    ring_TAIL: %u\n"
	       "    nr_queued: %u\n"
	       "  [STAT]\n"
	       "    nr_rx_data: %lu\n"
	       "    nr_rx_ack: %lu\n"
	       "    nr_rx_nack: %lu\n",
		ses->data_buffer_info_HEAD,
		ses->data_buffer_info_TAIL,
		nr_data_buffer_info(ses),
		ses->nr_rx_data,
		ses->nr_rx_ack,
		ses->nr_rx_nack);

	if (dump_rx_ring) {
		int i;
		struct buffer_info *info;
		char msg[256];

		printf("  [RX Pkts]\n");
		for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
			info = index_to_data_buffer_info(ses, i);
			if (info->buf) {
				__dump_packet_headers(info->buf, msg);
				printf("    [%d] %#lx size %u flags %#x %s\n",
					i, (unsigned long)info->buf,
					info->buf_size, info->flags, msg);
			}
		}
	}
}

static bool
handle_ack_nack_dequeue(struct gbn_header *hdr, struct session_gbn *ses_gbn,
		        bool is_ack)
{
	unsigned int i;
	unsigned int seq, seqnum_last;
	struct buffer_info *info;

	/* Update per-session stat */
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

		// TODO buf reg
		dprintf_INFO("buf %#lx\n", (unsigned long)info->buf);
		ret = raw_net_send(ses_net, info->buf, info->buf_size, NULL);
		if (ret < 0) {
			gbn_info("net_send error %d\n", ret);
			break;
		}
	}
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

	struct msg_buf *mb;
	struct eth_ack_packet {
		char	placeholder[GBN_HEADER_OFFSET];
		struct	gbn_header ack_header;
	} __packed *ack_pkt;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		dprintf_ERROR("no ses_gbn found. ses_net: %#lx\n",
			(unsigned long)ses_net);
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
		if (unlikely(!info)) {
			dprintf_INFO("packet dropped due to full ring buffer. "
				     "session_net srdid %u dstid %u\n",
				     get_local_session_id(ses_net),
				     get_remote_session_id(ses_net));
			return;
		}

		/*
		 * If there is no zerocopy, we need to copy
		 * the conent into the info->buf. Otherwise
		 * just save the pointer, the buffer is managed
		 * by the raw net layer.
		 */
		info->buf_size = buf_size;
		if (likely(raw_net_has_zerocopy))
			info->buf = packet;
		else
			memcpy(info->buf, packet, buf_size);

		/*
		 * For x86 TSO, as long as compiler reserves
		 * the order, the other cores are guaranteed
		 * to see above updates before the next one.
		 */
		barrier();
		__set_buffer_info_usable(info);
		return;
	}

	/*
	 * Otherwise we are dealing with normal traffic,
	 * i.e., packets targeting non-mgmt sessions.
	 * We will check seqnum accordingly.
	 */
	hdr = to_gbn_header(packet);
	seq = hdr->seqnum;

	/* Use the pre-registered buffer to send ACK pkt */
	mb = ses_gbn->mb_ack_reply;
	ack_pkt = (struct eth_ack_packet *)mb->buf;

	set_gbn_src_dst_session(&ack_pkt->ack_header,
				get_gbn_dst_session(hdr),
				get_gbn_src_session(hdr));

	if (likely(seq == atomic_load(&ses_gbn->seqnum_expect))) {
		/*
		 * Case 1: normal case
		 * seqnum is valid and as expected,
		 * send back ACK, enable response, and increase expected seqnum
		 */
		ses_gbn->nr_rx_data++;

		info = alloc_data_buffer_info(ses_gbn);
		if (unlikely(!info)) {
			dprintf_INFO("packet dropped due to full ring buffer. "
				     "session_net srdid %u dstid %u\n",
				     get_local_session_id(ses_net),
				     get_remote_session_id(ses_net));
			return;
		}

		/* See comments above */
		info->buf_size = buf_size;
		if (likely(raw_net_has_zerocopy))
			info->buf = packet;
		else
			memcpy(info->buf, packet, buf_size);
		barrier();
		__set_buffer_info_usable(info);

		/*
		 * XXX: YS
		 * What is the usage of this ack_enable?
		 * it seems it got updated for every data packet
		 */
		ses_gbn->ack_enable = true;

		/* Construct and send back ACK */
		ack_pkt->ack_header.type = GBN_PKT_ACK;
		ack_pkt->ack_header.seqnum = atomic_fetch_add(&ses_gbn->seqnum_expect, 1);

		inc_stat(STAT_NET_GBN_NR_TX_ACK);

		ret = raw_net_send_msg_buf(ses_net, mb, sizeof(*ack_pkt), NULL);
		if (unlikely(ret < 0)) {
			dprintf_ERROR("net_send error %d\n", ret);
			return;
		}
	} else if (ses_gbn->ack_enable) {
		if (seq > atomic_load(&ses_gbn->seqnum_expect)) {
			/*
			 * Case 2:
			 * seqnum invalid, if response is enabled,
			 * send back NACK and disable further response
			 */
			ses_gbn->ack_enable = false;
			ack_pkt->ack_header.type = GBN_PKT_NACK;
			ack_pkt->ack_header.seqnum = atomic_load(&ses_gbn->seqnum_expect) - 1;

			inc_stat(STAT_NET_GBN_NR_TX_NACK);
			ret = raw_net_send_msg_buf(ses_net, mb, sizeof(*ack_pkt), NULL);
			if (ret < 0) {
				dprintf_ERROR("net_send error %d\n", ret);
				return;
			}
		} else if (seq < atomic_load(&ses_gbn->seqnum_expect)) {
			/*
			 * Case 3:
			 * seqnum valid, but not as expected.
			 * if response is enabled, send back ACK
			 */
			ack_pkt->ack_header.type = GBN_PKT_ACK;
			ack_pkt->ack_header.seqnum = atomic_load(&ses_gbn->seqnum_expect) - 1;

			inc_stat(STAT_NET_GBN_NR_TX_ACK);
			ret = raw_net_send_msg_buf(ses_net, mb, sizeof(*ack_pkt), NULL);
			if (ret < 0) {
				dprintf_ERROR("net_send error %d\n", ret);
				return;
			}
		}
	}
}

int nr_recv_pkt = 0;

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
	unsigned int dst_sesid;
	void *recv_buf;
	size_t buf_size, max_buf_size = 0;
	int ret, node, cpu;

	/*
	 * If there is no zerocopy,
	 * we need to Bring Our Own Buffers.
	 */
	if (!raw_net_has_zerocopy) {
		max_buf_size = sysctl_link_mtu;
		recv_buf = malloc(max_buf_size);
		if (!recv_buf) {
			printf("%s(): Fail to alloc recv_buf\n", __func__);
			return NULL;
		}
	}

	ret = pin_cpu(gbn_polling_thread_cpu);
	if (ret) {
		dprintf_ERROR("fail to pin thread to CPU %d\n",
			gbn_polling_thread_cpu);
		return NULL;
	}

	legomem_getcpu(&cpu, &node);
	dprintf_CRIT("Global Polling Thread Running on CPU=%d NODE=%d\n", cpu, node);

	while (1) {
		if (unlikely(READ_ONCE(stop_gbn_poll_thread))) {
			dprintf_CRIT("Global Polling Thread Exit %d\n", 0);
			return NULL;
		}

		if (likely(raw_net_has_zerocopy)) {
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
		inc_stat(STAT_NET_GBN_NR_RX);

		gbn_hdr = to_gbn_header(recv_buf);
		dst_sesid = get_gbn_dst_session(gbn_hdr);

		/*
		 * Try to locate the net session. There are 3 cases:
		 * 1) local mgmt session, if dst session id is 0.
		 * 2) the matched session, if both IP+ID match.
		 * 3) NULL, if none of the above options fulfill
		 */
		ses_net = find_net_session(dst_sesid);
		if (unlikely(!ses_net)) {
			char str[INET_ADDRSTRLEN];
			struct in_addr in_addr;
			struct ipv4_hdr *ipv4_hdr;

			ipv4_hdr = to_ipv4_header(recv_buf);
			in_addr.s_addr = ipv4_hdr->src_ip;
			inet_ntop(AF_INET, &in_addr, str, sizeof(str));

			dprintf_ERROR("Session not found! src_ip: %s src_sesid: %u dst_sesid: %u\n",
				str, get_gbn_src_session(gbn_hdr), dst_sesid);
			dump_packet_headers(recv_buf, NULL);
			inc_stat(STAT_NET_GBN_NR_RX_ERROR_NO_SESSION);
			continue;
		}

#ifdef CONFIG_GBN_DEBUG
		{
			char packet_dump_str[256];

			dump_packet_headers(recv_buf, packet_dump_str);
			gbn_debug("new pkt: %s  buf_size: %zu nr_recv_pkt: %d\n",
				packet_dump_str, buf_size,
				nr_recv_pkt++);
		}
#endif

		switch (gbn_hdr->type) {
		case GBN_PKT_ACK:
			inc_stat(STAT_NET_GBN_NR_RX_ACK);
			handle_ack_packet(ses_net, recv_buf);
			break;
		case GBN_PKT_NACK:
			inc_stat(STAT_NET_GBN_NR_RX_NACK);
			handle_nack_packet(ses_net, recv_buf);
			break;
		case GBN_PKT_DATA:
			inc_stat(STAT_NET_GBN_NR_RX_DATA);
			handle_data_packet(ses_net, recv_buf, buf_size);
			break;
		default:
			inc_stat(STAT_NET_GBN_NR_RX_ERROR_UNKNOWN_TYPE);
			dprintf_ERROR("Unknown GBN type: %d\n", gbn_hdr->type);
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
 *
 * NOTE:
 * We are NOT COPYing the data to unack queue.
 * We are JUST saving th pointers.
 */
static inline int gbn_send_one(struct session_net *net,
			       void *buf, size_t buf_size, void *route)
{
	struct buffer_info *info;
	struct session_gbn *ses;
	int seqnum;
	struct gbn_header *hdr;
	bool unacked_buffer_empty __maybe_unused;

	/*
	 * This is not the best place to run this function.
	 * But placing here ensures all msgs are properly formatted.
	 * And could even be used to catch buggy callers.
	 */
	prepare_legomem_header(buf, buf_size);

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

	/* Try to alloc a new unack buffer and seqnum */
	info = alloc_unack_buffer_info(ses, &seqnum);
	if (!info)
		return -ENOMEM;

	/* Save into the pkt */
	hdr->seqnum = seqnum;

	info->buf = buf;
	info->buf_size = buf_size;

send:
	inc_stat(STAT_NET_GBN_NR_TX_DATA);
	return raw_net_send(net, buf, buf_size, route);
}

static __always_inline void
wait_data_buffer_usable(struct buffer_info *info)
{
	while (unlikely(!__test_buffer_info_usable(info)))
		;
}

/*
 * GBN receive a packet, i.e., dequeue a packet from the data buffer array.
 * Pakets in that array have been ack'ed and are ready for grab.
 *
 * Note that for a single session @net, only a single thread is supposed
 * to use this particular session. The behavior is undefined if multiple
 * threads call this function upon one session.
 *
 * @zerocopy:		if zerocopy is used. If yes, use @z_buf and @z_buf_size.
 * 			Otherwise use @buf and @buf_size.
 * @non_blocking:	if this is a non-blocking receive. If yes, return
 * 			immediately if there is no packet enqueued.
 */
static int
__gbn_receive_one(struct session_net *net,
		  void *buf, size_t buf_size,
		  void **z_buf, size_t *z_buf_size,
		  bool zerocopy, bool non_blocking)
{
	struct session_gbn *ses;
	struct buffer_info *info;
	int index;

	ses = (struct session_gbn *)net->transport_private;

	if (nr_data_buffer_info(ses) == 0) {
		struct timespec s, e;

		if (likely(non_blocking))
			return 0;

		clock_gettime(CLOCK_MONOTONIC, &s);
		while (unlikely(!nr_data_buffer_info(ses))) {
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) > GBN_RECEIVE_MAX_TIMEOUT_S) {
				dprintf_ERROR("Timeout %d s, sesid: %u TAIL %d HEAD %d\n",
					GBN_RECEIVE_MAX_TIMEOUT_S,
					get_local_session_id(net),
					ses->data_buffer_info_TAIL,
					ses->data_buffer_info_HEAD);
				print_backtrace();
				return -ETIMEDOUT;
			}
		}
	}

	index = ses->data_buffer_info_TAIL++;
	info = index_to_data_buffer_info(ses, index);

	/* Put the data back to the head if too small */
	if (unlikely(!zerocopy && (info->buf_size > buf_size))) {
		dprintf_ERROR("User recv buf is too small. "
			     "(pkt: %u recv_buf: %zu)\n",
			info->buf_size, buf_size);
		ses->data_buffer_info_TAIL--;
		return -ENOMEM;
	}

	wait_data_buffer_usable(info);

	if (zerocopy) {
		*z_buf = info->buf;
		*z_buf_size = info->buf_size;
	} else {
		memcpy(buf, info->buf, info->buf_size);
	}

	free_data_buffer_info(info);
	return info->buf_size;
}

static inline int gbn_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return __gbn_receive_one(net,
				 buf, buf_size,
				 NULL, NULL,
				 false,		/* zerocopy */
				 false);	/* non_blocking */
}

static inline int gbn_receive_one_nb(struct session_net *net,
				     void *buf, size_t buf_size)
{
	return __gbn_receive_one(net,
				 buf, buf_size,
				 NULL, NULL,
				 false,		/* zerocopy */
				 true);		/* non_blocking */
}

static inline int gbn_receive_one_zerocopy(struct session_net *net,
				  void **buf, size_t *buf_size)
{
	return __gbn_receive_one(net,
				 NULL, 0,
				 buf, buf_size,
				 true,		/* zerocopy */
				 false);	/* non_blocking */
}

static inline int
gbn_receive_one_zerocopy_nb(struct session_net *net, void **buf, size_t *buf_size)
{
	return __gbn_receive_one(net,
				 NULL, 0,
				 buf, buf_size,
				 true,		/* zerocopy */
				 true);		/* non_blocking */
}

#ifdef CONFIG_GBN_ENABLE_TIMER
static void gbn_timeout_handler(union sigval val)
{
	struct session_net *ses_net;
	struct session_gbn *ses_gbn;

	ses_net = (struct session_net *)val.sival_ptr;
	ses_gbn = (struct session_gbn *)ses_net->transport_private;

	dprintf_ERROR("Session %d timeout\n", ses_net->session_id);
	retrans_unack_buffer_info(ses_net, ses_gbn);
}
#endif

static int init_session_gbn(struct session_net *net, struct session_gbn *gbn)
{
	int i;
	struct buffer_info *info;
	struct sigevent timeout_event __maybe_unused = { 0 };
	int ret;

	/* TX: unack buffer */
	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_unack_buffer_info(gbn, i);
		memset(info, 0, sizeof(*info));
	}

	atomic_init(&gbn->seqnum_cur, 0);
	atomic_init(&gbn->seqnum_last, 0);

#ifdef CONFIG_GBN_ENABLE_TIMER
	/*
	 * Create timer for this gbn session
	 * Store net session in the sigevent struct
	 */
	timeout_event.sigev_notify = SIGEV_THREAD;
	timeout_event.sigev_value.sival_ptr = (void *)net;
	timeout_event.sigev_notify_function = gbn_timeout_handler;
	ret = timer_create(CLOCK_MONOTONIC, &timeout_event, &gbn->rt_timer);
	if (ret < 0) {
		ret = -errno;
		dprintf_ERROR("Failed to create timer %d\n", errno);
		goto out;
	}
#endif

	/* RX: incoming data list */
	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_data_buffer_info(gbn, i);
		memset(info, 0, sizeof(*info));

		/*
		 * If there is no zerocopy, we need to
		 * allocate the data buffers. The polling
		 * thread will copy the packet into this buffer
		 * instead of saving the pointer.
		 */
		if (!raw_net_has_zerocopy) {
			info->buf = malloc(sysctl_link_mtu);
			if (!info->buf) {
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	gbn->data_buffer_info_HEAD = 0;
	gbn->data_buffer_info_TAIL = 0;
	atomic_init(&gbn->seqnum_expect, 1);

	ret = 0;
out:
	return ret;
}

static void gbn_timer_delete(struct session_gbn *ses_gbn)
{
#ifdef CONFIG_GBN_ENABLE_TIMER
	timer_delete(ses_gbn->rt_timer);
#endif
}

static int
gbn_open_session(struct session_net *ses_net, struct endpoint_info *local_ei,
		 struct endpoint_info *remote_ei)
{
	struct session_gbn *ses_gbn;
	int ret;
	void *buf;
	struct msg_buf *mb;

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

	/*
	 * Prepare the ACK msg buffer beforehand.
	 */
	buf = malloc(sysctl_link_mtu);
	if (!buf) {
		gbn_timer_delete(ses_gbn);
		free(ses_gbn);
		ses_net->transport_private = NULL;
		return -ENOMEM;
	}

	mb = raw_net_reg_msg_buf(ses_net, buf, sysctl_link_mtu);
	if (!mb) {
		gbn_timer_delete(ses_gbn);
		free(buf);
		free(ses_gbn);
		ses_net->transport_private = NULL;
		return -ENOMEM;
	}

	ses_gbn->mb_ack_reply = mb;
	return 0;
}

static int gbn_close_session(struct session_net *ses_net)
{
	struct session_gbn *ses_gbn;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (ses_gbn) {
		gbn_debug("session %d, ack %ld, nack %ld\n",
			  ses_net->session_id, ses_gbn->nr_rx_ack,
			  ses_gbn->nr_rx_nack);
		gbn_timer_delete(ses_gbn);
		free(ses_gbn);
	}
	return 0;
}

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
		raw_net_has_zerocopy = true;
	else
		raw_net_has_zerocopy = false;

	dprintf_INFO("Raw net has zerocopy: %s\n", raw_net_has_zerocopy ? "YES" : "NO");

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
	.receive_one_zerocopy	= gbn_receive_one_zerocopy,
	.receive_one_zerocopy_nb= gbn_receive_one_zerocopy_nb,

	.reg_send_buf		= default_transport_reg_send_buf,
};
