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
#include <stdatomic.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>
#include "net.h"
#include "../core.h"

/*
 * The maximum number of unack'ed outgoing messages.
 * Organized as a ring.
 */
#define NR_BUFFER_INFO_SLOTS		(256)
#define GBN_RETRANS_TIMEOUT_US		(2000)
#define GBN_RECEIVE_TIMEOUT_S		(20)
#define GBN_TIMEOUT_CHECK_INTERVAL_MS	(5)

#define gbn_info(fmt, ...) \
	printf("%s(): " fmt, __func__, __VA_ARGS__)

#define gbn_debug(fmt, ...) \
	printf("%s(): " fmt, __func__, __VA_ARGS__)

static int polling_thread_created = 0;
static int timeout_watcher_created = 0;
static pthread_t polling_thread;
static pthread_t timeout_watcher_thread;

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
	struct timespec			next_timeout;

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
	/* set next timeout to infinity */
	ses->next_timeout.tv_sec = LONG_MAX;
	ses->next_timeout.tv_nsec = LONG_MAX;
}

/*
 * return if time t1 is greater then t2
 */
static __always_inline bool
time_greater_than(struct timespec *t1, struct timespec *t2)
{
	if (t1->tv_sec == t2->tv_sec)
		return t1->tv_nsec > t2->tv_nsec;
	else
		return t1->tv_sec > t2->tv_sec;
}

static __always_inline void
set_next_timeout(struct timespec *t)
{
	long ns;
	ns = t->tv_nsec + GBN_RETRANS_TIMEOUT_US * 1000;
	t->tv_nsec = ns % 1000000000;
	t->tv_sec += ns / 1000000000;
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
	if (hdr->type == pkt_type_ack)
		ses_gbn->nr_rx_ack++;
	else if (hdr->type == pkt_type_nack)
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
		else if (hdr->type == pkt_type_ack)
			set_next_timeout(&ses_gbn->next_timeout);

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
		gbn_debug("WARN. Recevied Seq: %u. less than Last Acked Seq: %u\n",
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
		set_next_timeout(&ses_gbn->next_timeout);
}

static void handle_ack_packet(struct session_net *ses_net, void *packet)
{
	struct gbn_header *hdr;
	struct session_gbn *ses_gbn;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_debug("ERROR: corrupted ses_gbn %p\n",
			ses_net);
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
		gbn_debug("ERROR: corrupted ses_gbn %p\n",
			ses_net);
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
static void handle_data_packet(struct session_net *ses_net, void *packet, size_t buf_size)
{
	struct gbn_header *hdr;
	struct session_gbn *ses_gbn;
	struct buffer_info *info;
	unsigned int seq;
	int ret;

	struct eth_ack_packet {
		char	placeholder[GBN_HEADER_OFFSET];
		struct	gbn_header ack_header;
	} __attribute__((packed)) ack;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_debug("ERROR: corrupted ses_gbn %p\n",
			ses_net);
		return;
	}

	hdr = to_gbn_header(packet);
	seq = hdr->seqnum;
	
	/*
	 * seqnum is valid and as expected, send back ACK, enable response,
	 * and increase expected seqnum
	 */
	if (likely(seq == atomic_load(&ses_gbn->seqnum_expect))) {
		ses_gbn->nr_rx_data++;

		info = alloc_data_buffer_info(ses_gbn);
		if (!info)
			return;

		/*
		 * XXX
		 *
		 * modify this with top level when we have a better
		 * way of handling this case.
		 */
		if (unlikely(!raw_net_ops->receive_one_zerocopy)) {
			if (!info->buf) {
				info->buf = malloc(buf_size);
				if (!info->buf)
					return;
			}
			memcpy(info->buf, packet, buf_size);
		} else
			info->buf = packet;
		info->buf_size = buf_size;

		ses_gbn->ack_enable = true;

		ack.ack_header.type = pkt_type_ack;
		ack.ack_header.seqnum = atomic_fetch_add(&ses_gbn->seqnum_expect, 1);
		ret = raw_net_send(ses_net, &ack, sizeof(ack), NULL);
		if (ret < 0) {
			gbn_info("net_send error %d\n", ret);
			return;
		}

		enqueue_data_buffer_info_tail(ses_gbn, info);
	} else if (ses_gbn->ack_enable) {
		if (seq > atomic_load(&ses_gbn->seqnum_expect)) {
			/*
			 * seqnum invalid, if response is enabled,
			 * send back NACK and disable further response
			 */
			ses_gbn->ack_enable = false;
			ack.ack_header.type = pkt_type_nack;
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
			ack.ack_header.type = pkt_type_ack;
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
	struct ipv4_hdr *ipv4_hdr;
	unsigned int remote_ip, session_id;
	void *recv_buf;
	size_t buf_size, max_buf_size;
	bool use_zerocopy = true;
	int ret;

	if (unlikely(!raw_net_ops->receive_one_zerocopy)) {
		printf("%s(): No zerocopy implemented.\n", __func__);
		use_zerocopy = false;
		max_buf_size = sysctl_link_mtu;
		recv_buf = malloc(max_buf_size);
		if (!recv_buf) {
			printf("Fail to allocate recv_buf\n");
			return NULL;
		}
	}

	while (1) {
		if (likely(use_zerocopy)) {
			ret = raw_net_receive_zerocopy(&recv_buf, &buf_size);
			if (unlikely(ret < 0)) {
				gbn_info("zerocopy receive error %d\n", ret);
				exit(1);
			} else if (ret == 0)
				continue;
		} else {
			buf_size = raw_net_receive(recv_buf, max_buf_size);
			if (unlikely(buf_size < 0)) {
				gbn_info("receive error %ld\n", buf_size);
				exit(1);
			} else if (buf_size == 0)
				continue;
		}
		
		gbn_hdr = to_gbn_header(recv_buf);
		ipv4_hdr = to_ipv4_header(recv_buf);

		/*
		 * TODO
		 *
		 * I'm not adding a session ID to the GBN header now
		 * because we need to consider the FPGA side stack layout
		 * many bits shitfing might need changes.
		 */
		session_id = 0;
		remote_ip = ipv4_hdr->src_ip;

		ses_net = find_net_session(remote_ip, session_id);
		if (unlikely(!ses_net)) {
			gbn_debug("Session not found: %x %u\n",
				remote_ip, session_id);
			continue;
		}

		switch (gbn_hdr->type) {
		case pkt_type_ack:
			handle_ack_packet(ses_net, recv_buf);
			break;
		case pkt_type_nack:
			handle_nack_packet(ses_net, recv_buf);
			break;
		case pkt_type_data:
			handle_data_packet(ses_net, recv_buf, buf_size);
			break;
		default:
			gbn_info("WARN: Unknown GBN packet type: %d\n",
				gbn_hdr->type);
			break;
		};
	}
	return NULL;
}

/*
 * TODO
 * If a session is used by multiple threads,
 * we need to maintain some sort of ordering here.
 * The lower seqnum ones should send out first.
 */
static inline int gbn_send_one(struct session_net *net,
				  void *buf, size_t buf_size, void *route)
{
	struct buffer_info *info;
	struct session_gbn *ses;
	struct gbn_header *hdr;
	struct timespec e;
	int seqnum;
	bool unacked_buffer_empty;

	ses = (struct session_gbn *)net->transport_private;
	BUG_ON(!ses);

	/*
	 * We check if unacked buffer is empty before grabbing a slot,
	 * since grabing a slot will increase seqnum_cur and the 
	 * buffer can never be empty if the seqnum_cur is increased
	 */
	unacked_buffer_empty = !nr_unack_buffer(ses);

	info = alloc_unack_buffer_info(ses, &seqnum);
	if (!info)
		return -ENOMEM;

	/* cook the seqnum and packet type */
	hdr = to_gbn_header(buf);
	hdr->type = pkt_type_data;
	hdr->seqnum = seqnum;

	info->buf = buf;
	info->buf_size = buf_size;

	/*
	 * We do not reset timeout for every packet sent out.
	 * Timeout is only (re)set when the unack buffer is originally empty
	 */
	if (unacked_buffer_empty) {
		clock_gettime(CLOCK_MONOTONIC, &e);
		set_next_timeout(&e);
		ses->next_timeout = e;
	}

	return raw_net_send(net, buf, buf_size, route);
}

/*
 * TODO
 *    Similariy, if a session is used by multiple threads,
 *    we need to make sure a sender can assocaite the request with reply.
 *    Some sort of handle is needed.
 */
static inline int gbn_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
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

	/* This is the only memcpy we have along the stack. */
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

static void init_session_gbn(struct session_gbn *ses)
{
	int i;
	struct buffer_info *info;

	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_unack_buffer_info(ses, i);
		memset(info, 0, sizeof(*info));
		INIT_LIST_HEAD(&info->list);
	}
	atomic_init(&ses->seqnum_cur, 0);
	atomic_init(&ses->seqnum_last, 0);
	disable_timeout(ses);

	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_data_buffer_info(ses, i);
		memset(info, 0, sizeof(*info));
		INIT_LIST_HEAD(&info->list);
	}
	atomic_init(&ses->data_buffer_info_HEAD, 0);
	atomic_init(&ses->seqnum_expect, 1);

	INIT_LIST_HEAD(&ses->data_list);
	pthread_spin_init(&ses->data_lock, PTHREAD_PROCESS_PRIVATE);
	ses->nr_data = 0;
}

static int
gbn_open_session(struct session_net *ses_net, struct endpoint_info *local_ei,
		 struct endpoint_info *remote_ei)
{
	struct session_gbn *ses_gbn;

	ses_gbn = malloc(sizeof(struct session_gbn));
	if (!ses_gbn)
		return -ENOMEM;
	ses_net->transport_private = ses_gbn;
	init_session_gbn(ses_gbn);

	return 0;
}

static int gbn_close_session(struct session_net *ses_net)
{
	struct session_gbn *ses_gbn;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (ses_gbn)
		free(ses_gbn);
	return 0;
}

/*
 * This is go-back-N transport layer's timeout watcher thread.
 * It wakes up every 0.5ms to check if a timeout event happends.
 * If timeout, it will start a retransmission.
 * XXX
 * We maynot want to use a signal-like approach, since it involve with kernel
 */
static void *gbn_timeout_watcher_func(void *_unused)
{
	/*
	 * XXX Yizhou
	 * This thread will not work as expected in multi session case.
	 * We need to use per-session timers now.
	 */
#if 0
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
#endif
	return NULL;
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

static int create_timeout_watcher_thread(void)
{
	int ret;

	if (timeout_watcher_created)
		return 0;

	ret = pthread_create(&timeout_watcher_thread, NULL,
			     gbn_timeout_watcher_func, NULL);	
	if (ret) {
		printf("GBN: Fail to create the timeout wather thread\n");
		return ret;
	}
	printf("Created the go-back-N reliable net timeout watcher thread\n");
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

	ret = create_timeout_watcher_thread();
	if (ret) {
		printf("Fail to create GBN timeout watcher thread\n");
		return ret;
	}
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
