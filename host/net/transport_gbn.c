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
#include <stdatomic.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>
#include "net.h"

/*
 * The maximum number of unack'ed outgoing messages.
 * Organized as a ring.
 */
#define NR_BUFFER_INFO_SLOTS	(256)

#define gbn_info(fmt, ...) \
	printf("%s(): " fmt, __func__, __VA_ARGS__)

#define gbn_debug(fmt, ...) \
	printf("%s(): " fmt, __func__, __VA_ARGS__)

static int thread_created = 0;
static pthread_t polling_thread;

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
	 *                  ^ (unack'ed slots)  ^
	 *    (free slots) tail                head   (free slots)
	 *                  ^                   ^
	 *             (seqnum_last)        (seqnum_cur)
	 *
	 * Sequence numeber starts from 0, and increase monotonically.
	 * seqnum_cur is the head, seqnum_last is the tail.
	 */
	struct buffer_info		unack_buffer_info_ring[NR_BUFFER_INFO_SLOTS];
	atomic_int			seqnum_cur;
	atomic_int			seqnum_last;

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

static inline void init_session_gbn(struct session_gbn *ses)
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

	for (i = 0; i < NR_BUFFER_INFO_SLOTS; i++) {
		info = index_to_data_buffer_info(ses, i);
		memset(info, 0, sizeof(*info));
		INIT_LIST_HEAD(&info->list);
	}
	atomic_init(&ses->data_buffer_info_HEAD, 0);

	INIT_LIST_HEAD(&ses->data_list);
	pthread_spin_init(&ses->data_lock, PTHREAD_PROCESS_PRIVATE);
	ses->nr_data = 0;
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
	int seq;

	seq = atomic_fetch_add(&ses->seqnum_cur, 1);
	info = index_to_unack_buffer_info(ses, seq);

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

	/* Prepare the new slow */
	set_buffer_info_allocated(info);
	*seqnum = seq;
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

	/* Prepare the new slow */
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
 * i.e., it must sit in the seqnum_last.
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

static void handle_ack_packet(void *packet)
{
	struct gbn_header *hdr;
	struct session_net *ses_net;
	struct session_gbn *ses_gbn;
	unsigned int seq;
	struct buffer_info *info;

	/*
	 * Find the connection info from the packet
	 * for now just use the global one
	 */
	ses_net = find_session(packet);
	if (unlikely(!ses_net)) {
		gbn_debug("ERROR: no valid connection %p\n",
			packet);
		return;
	}

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_debug("ERROR: corrupted ses_gbn %p\n",
			ses_net);
		return;
	}
	ses_gbn->nr_rx_ack++;

	hdr = to_gbn_header(packet);
	seq = hdr->seqnum;
	info = index_to_unack_buffer_info(ses_gbn, seq);

	/* Verify if the seq number is generally valid */
	if (unlikely(!buffer_info_allocated(info))) {
		gbn_debug("ERROR. Recevied Seq: %u. Expected Seq: %u\n",
			seq, atomic_load(&ses_gbn->seqnum_last));
		return;
	}

	/*
	 * Then check if this is the expected sequence number
	 * TODO:
	 * Protocol: If not, send sth back to board?
	 */
	if (unlikely(seq != atomic_load(&ses_gbn->seqnum_last))) {
		gbn_debug("Packet Loss: Recevied Seq: %u. Expected Seq: %u\n",
			seq, atomic_load(&ses_gbn->seqnum_last));
		return;
	}

	/*
	 * The ACK packet is valid.
	 * Free the unack buffer and move expected seqnum forward.
	 */
	atomic_fetch_add(&ses_gbn->seqnum_last, 1);
	free_unack_buffer_info(info);
}

static void handle_nack_packet(void *packet)
{
	/*
	 * TODO:
	 *
	 * Perform the actions following the protocol.
	 * Resend necessary packets according seqnum_cur, seqnum_last, and NACK seq.
	 */
}

/*
 * Called by polling thread.
 * Push new data into cached ring buffer list.
 */
static void handle_data_packet(void *packet, size_t buf_size)
{
	struct session_net *ses_net;
	struct session_gbn *ses_gbn;
	struct buffer_info *info;

	ses_net = find_session(packet);
	if (unlikely(!ses_net)) {
		gbn_debug("ERROR: no valid connection %p\n",
			packet);
		return;
	}

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	if (unlikely(!ses_gbn)) {
		gbn_debug("ERROR: corrupted ses_gbn %p\n",
			ses_net);
		return;
	}
	ses_gbn->nr_rx_data++;

	info = alloc_data_buffer_info(ses_gbn);
	if (!info)
		return;

	/*
	 * TODO
	 * Better buffer mgmt
	 * should avoid the copy here.
	 */
	info->buf = malloc(buf_size);
	if (!info->buf)
		return;
	memcpy(info->buf, packet, buf_size);
	info->buf_size = buf_size;

	enqueue_data_buffer_info_tail(ses_gbn, info);
}

/*
 * This is Go-Back-N transport layer's polling thread.
 * It invokes lower level raw network interface to receive new packets.
 * It then inspects the packet contents, either clear unack'ed buffers,
 * or cache the data packets and generate ACK.
 */
static void *gbn_poll_func(void *arg)
{
	struct session_net *ses_net;
	struct gbn_header *hdr;
	void *recv_buf;
	int buf_size, max_buf_size;

	ses_net = (struct session_net *)arg;

	max_buf_size = sysctl_link_mtu;
	recv_buf = malloc(max_buf_size);
	if (!recv_buf) {
		printf("Fail to allocate recv_buf\n");
		return NULL;
	}

	while (1) {
		/*
		 * TODO:
		 * there should be generic data structure per device
		 * and then some data structures per connection
		 * as this particular case, we should use the generic one
		 */
		buf_size = raw_net_receive(ses_net, recv_buf, max_buf_size);
		if (unlikely(buf_size < 0)) {
			gbn_info("receive error %d\n", buf_size);
			exit(1);
		} else if (buf_size == 0)
			continue;

		hdr = to_gbn_header(recv_buf);
		switch (hdr->type) {
		case pkt_type_ack:
			handle_ack_packet(recv_buf);
			break;
		case pkt_type_nack:
			handle_nack_packet(recv_buf);
			break;
		case pkt_type_data:
			handle_data_packet(recv_buf, buf_size);
			break;
		default:
			gbn_info("WARN: Unknown GBN packet type: %d\n",
				hdr->type);
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
				  void *buf, size_t buf_size)
{
	struct buffer_info *info;
	struct session_gbn *ses;
	int seqnum;

	ses = (struct session_gbn *)net->transport_private;
	BUG_ON(!ses);

	info = alloc_unack_buffer_info(ses, &seqnum);
	if (!info)
		return -ENOMEM;
	info->buf = buf;
	info->buf_size = buf_size;

	return raw_net_send(net, buf, buf_size);
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
			if ((e.tv_sec - s.tv_sec) > 2) {
				printf("gdn receive: timeout\n");
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

	/* This should be removed too! */
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

/*
 * For now, we only create one global polling thread per machine.
 * If this becomes a bottleneck, we may well change this.
 */
static int create_polling_thread(struct session_net *ses)
{
	int ret;

	/*
	 * This is not SMP safe, esp if there are multiple
	 * threads running this func for the first time
	 */
	if (thread_created)
		return 0;

	ret = pthread_create(&polling_thread, NULL, gbn_poll_func, ses);
	if (ret) {
		printf("GBN: Fail to create the polling thread\n");
		return ret;
	}
	printf("Created the go-back-N reliable net polling thread\n");
	return 0;
}

static inline int gbn_init(struct session_net *ses)
{
	struct session_gbn *ses_gbn;
	int ret;

	if (!ses)
		return -EINVAL;

	if (!ses->raw_net_private) {
		printf("ERROR: Please init raw_net first\n");
		return -EINVAL;
	}

	ses_gbn = malloc(sizeof(struct session_gbn));
	if (!ses_gbn)
		return -ENOMEM;
	ses->transport_private = ses_gbn;
	init_session_gbn(ses_gbn);

	/* One global polling thread */
	ret = create_polling_thread(ses);
	if (ret) {
		printf("Fail to create GBN polling thread\n");
		return ret;
	}
	return 0;
}

struct transport_net_ops transport_gbn_ops = {
	.name			= "transport_rel_gbn",
	.send_one		= gbn_send_one,
	.receive_one		= gbn_receive_one,
	.receive_one_nb		= gbn_receive_one_nb,
	.init			= gbn_init,
};
