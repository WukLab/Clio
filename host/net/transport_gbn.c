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
#define CONFIG_GBN_DUMP_RX
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
	atomic_long			data_buffer_info_HEAD;
	unsigned long			data_buffer_info_TAIL;

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
	struct buffer_info *info = NULL;
	int index;

	index = atomic_fetch_add(&ses->seqnum_cur, 1);
#if 0
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
#endif
	*seqnum = index + 1;
	return info;
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
	       "  [STAT]\n"
	       "    nr_rx_data: %lu\n"
	       "    nr_rx_ack: %lu\n"
	       "    nr_rx_nack: %lu\n",
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

static __always_inline bool
handle_ack_nack_dequeue(struct gbn_header *hdr, struct session_gbn *ses_gbn,
		        bool is_ack)
{
	unsigned int i;
	unsigned int seq, seqnum_last;
	struct buffer_info *info;

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

__used
static __always_inline void
handle_ack_packet(struct session_net *ses_net, struct session_gbn *ses_gbn, void *packet)
{
	struct gbn_header *hdr;
	hdr = to_gbn_header(packet);
	handle_ack_nack_dequeue(hdr, ses_gbn, true);
}

static __always_inline void
handle_nack_packet(struct session_net *ses_net, struct session_gbn *ses_gbn, void *packet)
{
	struct gbn_header *hdr;
	hdr = to_gbn_header(packet);

	/* If received packet is valid, retransmit */
	if (handle_ack_nack_dequeue(hdr, ses_gbn, false))
		retrans_unack_buffer_info(ses_net, ses_gbn);
}

/*
 * Called by polling thread.
 * Push new data into cached ring buffer list.
 */
static void handle_data_packet(struct session_net *ses_net,
			       struct session_gbn *ses_gbn,
			       void *packet, size_t buf_size)
{
	struct gbn_header *hdr;
	unsigned int seq;
	int ret;

	struct msg_buf *mb;
	struct eth_ack_packet {
		char	placeholder[GBN_HEADER_OFFSET];
		struct	gbn_header ack_header;
	} __packed *ack_pkt;

	/*
	 * Session 0 is unreliable.
	 * The sender skipped unack buffer,
	 * and we, as the receiver, do not need to send ACK.
	 */
	if (unlikely(test_management_session(ses_net)))
		return;

	hdr = to_gbn_header(packet);
	seq = hdr->seqnum;

	if (likely(seq == atomic_load(&ses_gbn->seqnum_expect))) {
		/*
		 * Construct and send back ACK
		 * We only sent back an ACK every X packets.
		 */
		unsigned int out_seqnum;

		//ses_gbn->ack_enable = true;
		out_seqnum = atomic_fetch_add(&ses_gbn->seqnum_expect, 1);

		if (out_seqnum % 100 == 0) {
			/* Use the pre-registered buffer to send ACK pkt */
			mb = ses_gbn->mb_ack_reply;
			ack_pkt = (struct eth_ack_packet *)mb->buf;
			set_gbn_src_dst_session(&ack_pkt->ack_header,
						get_gbn_dst_session(hdr),
						get_gbn_src_session(hdr));

			ack_pkt->ack_header.seqnum = out_seqnum;
			ack_pkt->ack_header.type = GBN_PKT_ACK;
			ret = raw_net_send_msg_buf(ses_net, mb, sizeof(*ack_pkt), NULL);
			if (unlikely(ret < 0)) {
				dprintf_ERROR("net_send error %d\n", ret);
				return;
			}
			inc_stat(STAT_NET_GBN_NR_TX_ACK);
		}
	} else if (ses_gbn->ack_enable) {
		/* Use the pre-registered buffer to send ACK pkt */
		mb = ses_gbn->mb_ack_reply;
		ack_pkt = (struct eth_ack_packet *)mb->buf;
		set_gbn_src_dst_session(&ack_pkt->ack_header,
					get_gbn_dst_session(hdr),
					get_gbn_src_session(hdr));

		if (seq > atomic_load(&ses_gbn->seqnum_expect)) {
			/*
			 * Case 2:
			 * seqnum invalid, if response is enabled,
			 * send back NACK and disable further response
			 */
			dprintf_ERROR("WARN: received invalid seqnum %u\n", seq);

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
			dprintf_ERROR("WARN: received invalid seqnum %u\n", seq);

			ack_pkt->ack_header.type = GBN_PKT_ACK;
			ack_pkt->ack_header.seqnum = atomic_load(&ses_gbn->seqnum_expect) - 1;

			inc_stat(STAT_NET_GBN_NR_TX_ACK);
			ret = raw_net_send_msg_buf(ses_net, mb, sizeof(*ack_pkt), NULL);
			if (ret < 0) {
				dprintf_ERROR("net_send error %d\n", ret);
				return;
			}
		}
	} else {
		dprintf_ERROR("Not an expected ACK seqnum %u\n", seq);
	};
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
static int gbn_send_one(struct session_net *net, void *buf,
			size_t buf_size, void *route)
{
	struct session_gbn *ses;
	int seqnum;
	struct gbn_header *hdr;
	struct buffer_info *info __maybe_unused;

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
	alloc_unack_buffer_info(ses, &seqnum);
#if 0
	info = alloc_unack_buffer_info(ses, &seqnum);
	if (!info)
		return -ENOMEM;
	info->buf = buf;
	info->buf_size = buf_size;
#endif

	hdr->seqnum = seqnum;

send:
	inc_stat(STAT_NET_GBN_NR_TX_DATA);
	return raw_net_send(net, buf, buf_size, route);
}

static int gbn_send_one_msg_buf(struct session_net *net, struct msg_buf *mb,
				size_t buf_size, void *route)
{
	struct session_gbn *ses;
	int seqnum;
	struct gbn_header *hdr;
	struct buffer_info *info __maybe_unused;
	void *buf;

	/*
	 * This is not the best place to run this function.
	 * But placing here ensures all msgs are properly formatted.
	 * And could even be used to catch buggy callers.
	 */
	buf = mb->buf;
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

	if (test_management_session(net))
		goto send;

	ses = (struct session_gbn *)net->transport_private;
	BUG_ON(!ses);

	/* Try to alloc a new unack buffer and seqnum */
	alloc_unack_buffer_info(ses, &seqnum);
#if 0
	info = alloc_unack_buffer_info(ses, &seqnum);
	if (!info)
		return -ENOMEM;
	info->buf = buf;
	info->buf_size = buf_size;
#endif

	hdr->seqnum = seqnum;

send:
	inc_stat(STAT_NET_GBN_NR_TX_DATA);
	return raw_net_send_msg_buf(net, mb, buf_size, route);
}

/*
 * @zerocopy:		if zerocopy is used. If yes, use @z_buf and @z_buf_size.
 * 			Otherwise use @buf and @buf_size.
 * @non_blocking:	if this is a non-blocking receive. If yes, return
 * 			immediately if there is no packet enqueued.
 */
static int
__gbn_receive_one(struct session_net *ses_net,
		  void *buf, size_t buf_size,
		  void **z_buf, size_t *z_buf_size,
		  bool zerocopy, bool non_blocking)
{
	struct session_net *dst_ses_net;
	struct session_gbn *ses_gbn;
	void *recv_buf;
	size_t recv_size;
	struct gbn_header *gbn_hdr;
	unsigned int dst_sesid __maybe_unused;
	int ret;

	ses_gbn = (struct session_gbn *)ses_net->transport_private;
	BUG_ON(!ses_gbn);

retry:
	ret = raw_net_receive_zerocopy(ses_net, &recv_buf, &recv_size);
	if (ret < 0) {
		dprintf_ERROR("raw net failed %d\n", ret);
		return ret;
	} else if (ret == 0) {
		if (non_blocking)
			return 0;
		goto retry;
	}

	gbn_hdr = to_gbn_header(recv_buf);

#if 1
	dst_sesid = get_gbn_dst_session(gbn_hdr);
	dst_ses_net = find_net_session(dst_sesid);
	if (unlikely(dst_ses_net != ses_net)) {
		char packet_dump_str[256];
		struct session_raw_verbs *ses_verbs;


		ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
		dump_packet_headers(recv_buf, packet_dump_str);
		dprintf_ERROR("RX Wrong Session. Calling session: local_sesid %u remote_sesid %u. Verbs: qpn: %u rx_udp_port %u \n\n"
			      "\t pkt -> %s \n\n",
			get_local_session_id(ses_net), get_remote_session_id(ses_net),
			ses_verbs->qp->qp_num, ses_verbs->rx_udp_port, packet_dump_str);
	}
#endif

#ifdef CONFIG_GBN_DUMP_RX
	{
		char packet_dump_str[256];
		dump_packet_headers(recv_buf, packet_dump_str);
		dprintf_CRIT("RX: %s size: %zu\n", packet_dump_str, recv_size);
	}
#endif

	switch (gbn_hdr->type) {
	case GBN_PKT_ACK:
		/*
		 * If it is an ACK, we need to grab a new one.
		 * Same for NACK.
		 */
		inc_stat(STAT_NET_GBN_NR_RX_ACK);
		//handle_ack_packet(ses_net, ses_gbn, recv_buf);
		goto retry;
	case GBN_PKT_NACK:
		inc_stat(STAT_NET_GBN_NR_RX_NACK);
		handle_nack_packet(ses_net, ses_gbn, recv_buf);
		goto retry;
	case GBN_PKT_DATA:
		inc_stat(STAT_NET_GBN_NR_RX_DATA);
		handle_data_packet(ses_net, ses_gbn, recv_buf, recv_size);
		break;
	default:
		inc_stat(STAT_NET_GBN_NR_RX_ERROR_UNKNOWN_TYPE);
		dprintf_ERROR("Unknown GBN type: %x\n", gbn_hdr->type);
		return -EIO;
	};

	if (likely(zerocopy)) {
		z_buf[0] = recv_buf;
		z_buf_size[0] = recv_size;
	} else {
		memcpy(buf, recv_buf, recv_size);
	}
	return recv_size;
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

	atomic_init(&gbn->seqnum_expect, 1);

	ret = 0;
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

static int gbn_init_once(struct endpoint_info *local_ei)
{
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
	.send_one_msg_buf	= gbn_send_one_msg_buf,

	.receive_one		= gbn_receive_one,
	.receive_one_nb		= gbn_receive_one_nb,
	.receive_one_zerocopy	= gbn_receive_one_zerocopy,
	.receive_one_zerocopy_nb= gbn_receive_one_zerocopy_nb,

	.reg_send_buf		= default_transport_reg_send_buf,
};
