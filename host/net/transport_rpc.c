/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Unreliable RPC Transport Layer with Congestion Control.
 */

#include <stdatomic.h>
#include <time.h>
#include <pthread.h>
#include <uapi/compiler.h>
#include <uapi/list.h>
#include <uapi/net_header.h>
#include "net.h"
#include "../core.h"

#ifdef CONFIG_RPC_DEBUG
# define rpc_debug(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__)
#else
# define rpc_debug(fmt, ...) do { } while (0)
#endif

#define rpc_info(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__)

#define RPC_WAIT_CREDIT_TIMEOUT_S	(2)
#define NR_LATENCY_INFO_SLOTS		(256)
#define TARGET_DELAY_NS			(5000)
#define SEND_CREDIT_MAX			(4096)

/* 
 * congestion control parameters
 * ad-hoc value
 */
unsigned int default_credit_send = 4096;
unsigned int default_credit_recv = 4096;
int ai = 1;
double md = 0.5;
double beta = 0.8;

/*
 * This is a global variable controlling the buffer mgmt behavior.
 * If underlying raw net layer supports zerocopy receive, we will NOT
 * need to manage buffers, simply saving the pointer given by raw net.
 * Otherwise, we need to create buffer and a lot copies are involved.
 */
static bool raw_net_has_zerocopy __read_mostly;

struct per_board_send_state {
	unsigned int		board_ip;
	atomic_int		send_credit;

	/*
	 * Referene count of sessions using this board. If this
	 * decreases to 0, we will delete this struct
	 */
	atomic_int		session_cnt;
	struct list_head	list;
} __aligned(64);

/*
 * A global varible to store per host receive credits and per board congestion control
 * states. All sessions should refer to this variable when doing congestion
 * control.
 */
static struct congestion_control_states {
	struct list_head	per_board_states;
	atomic_int		recv_credit;
	pthread_spinlock_t	lock;
} __aligned(64) global_cc_states;

struct session_rpc {
	struct per_board_send_state	*send_state;
	struct timespec			latency_info_ring[NR_LATENCY_INFO_SLOTS];
	atomic_uint			ring_head;
	atomic_uint			ring_tail;
} __aligned(64);

static __always_inline void
set_send_timestamp(struct session_rpc *ses_rpc)
{
	unsigned int index;

	index = atomic_fetch_add(&ses_rpc->ring_head, 1) % NR_LATENCY_INFO_SLOTS;
	clock_gettime(CLOCK_MONOTONIC, &ses_rpc->latency_info_ring[index]);
}

static __always_inline unsigned int
get_last_delay(struct session_rpc *ses_rpc)
{
	unsigned int index;
	unsigned int interval_ns;
	struct timespec *s, e;

	index = atomic_fetch_add(&ses_rpc->ring_tail, 1) % NR_LATENCY_INFO_SLOTS;
	s = &ses_rpc->latency_info_ring[index];

	clock_gettime(CLOCK_MONOTONIC, &e);

	if (likely(e.tv_sec == s->tv_sec))
		interval_ns = e.tv_nsec - s->tv_nsec;
	else
		interval_ns = e.tv_nsec + 1000000000 - s->tv_nsec + (e.tv_sec - s->tv_sec - 1) * 1000000000;
	
	return interval_ns;
}

static __always_inline int
consume_send_credit(struct per_board_send_state *send_state)
{
	if (unlikely(atomic_fetch_sub(&send_state->send_credit, 1) < 1)) {
		struct timespec s, e;
		clock_gettime(CLOCK_MONOTONIC, &s);
		while (1) {
			if (likely(atomic_load(&send_state->send_credit) >= 0))
				break;
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) >= RPC_WAIT_CREDIT_TIMEOUT_S) {
				rpc_info("Wait for send credit timeout (%d s). "
					 "Probably there is no reply from other side",
					 RPC_WAIT_CREDIT_TIMEOUT_S);
			}
			return -ETIMEDOUT;
		}
	}
	return 0;
}

static __always_inline int
consume_recv_credit()
{
	if (unlikely(atomic_fetch_sub(&global_cc_states.recv_credit, 1) < 1)) {
		struct timespec s, e;
		clock_gettime(CLOCK_MONOTONIC, &s);
		while (1) {
			if (likely(atomic_load(&global_cc_states.recv_credit) >= 0))
				break;
			clock_gettime(CLOCK_MONOTONIC, &e);
			if ((e.tv_sec - s.tv_sec) >=
			    RPC_WAIT_CREDIT_TIMEOUT_S) {
				rpc_info("Wait for receive credit timeout (%d s). "
					"Probably there is no reply from other side",
					RPC_WAIT_CREDIT_TIMEOUT_S);
			}
			return -ETIMEDOUT;
		}
	}
	return 0;
}

/*
 * TODO: latency based congestion control
 * Adjust the send window based on the RTT of a request
 */
static void adjust_send_window(struct per_board_send_state *send_state, unsigned int delay)
{
	if (delay < TARGET_DELAY_NS) {
		if (atomic_load(&send_state->send_credit) < SEND_CREDIT_MAX)
			atomic_fetch_add(&send_state->send_credit, ai);
	} else {
		int delta = delay - TARGET_DELAY_NS;
		int cur_send_credit = atomic_load(&send_state->send_credit);
		atomic_store(&send_state->send_credit,
			     (int)(max(1 - beta * ((double)delta / (double)delay), md) * (double)cur_send_credit));
	}
}

static __always_inline void
refill_send_credit(struct per_board_send_state *send_state)
{
	atomic_fetch_add(&send_state->send_credit, 1);
}

static __always_inline void
refill_recv_credit()
{
	atomic_fetch_add(&global_cc_states.recv_credit, 1);
}

static int rpc_send_one(struct session_net *net, void *buf,
			size_t buf_size, void *route)
{
	struct session_rpc *ses;
	int ret;

	/*
	 * This is not the best place to run this function.
	 * But placing here ensures all msgs are properly formatted.
	 * And could even be used to catch buggy callers.
	 */
	prepare_legomem_header_with_sesid(buf, buf_size, net->session_id);

	ses = (struct session_rpc *)net->transport_private;
	BUG_ON(!ses);

	/* don't do congestion control on mgmt session */
	if (unlikely(test_management_session(net)))
		goto send;

	ret = consume_send_credit(ses->send_state);
	if (ret)
		return ret;
	ret = consume_recv_credit();
	if (ret)
		return ret;

	set_send_timestamp(ses);
send:
	return raw_net_send(net, buf, buf_size, route);
}

/*
 * @zerocopy:		if zerocopy is used. If yes, use @z_buf and @z_buf_size.
 * 			Otherwise use @buf and @buf_size.
 * @non_blocking:	if this is a non-blocking receive. If yes, return
 * 			immediately if there is no packet enqueued.
 */
static int
__rpc_receive_one(struct session_net *ses_net,
		  void *buf, size_t buf_size,
		  void **z_buf, size_t *z_buf_size,
		  bool zerocopy, bool non_blocking)
{
	void *recv_buf;
	size_t recv_size;
	int ret;
	struct lego_header *lego_hdr;
	unsigned int rpc_sesid __maybe_unused;
	struct session_net *rpc_ses_net __maybe_unused;
	struct session_rpc *ses_rpc;
	unsigned int delay_ns;

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

	lego_hdr = to_lego_header(recv_buf);

	/* sanity check */
	rpc_sesid = lego_hdr->src_sesid;
	rpc_ses_net = find_net_session(rpc_sesid);
	if (unlikely(rpc_ses_net != ses_net)) {
		char packet_dump_str[256];
		struct session_raw_verbs *ses_verbs;

		/*
		 * If this happens, it means there is some issue
		 * with the received packet's UDP port. E.g., dst_sesid+base_udp_port.
		 * Something wrong about the udp port setup code?
		 */
		ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
		dump_packet_headers(recv_buf, packet_dump_str);
		dprintf_ERROR(
			"RX wrong session. Calling session %u. Resp session %u. Verbs: qpn: %u rx_udp_port %u \n\n"
			"\t pkt -> %s \n\n",
			get_local_session_id(ses_net), rpc_sesid, ses_verbs->qp->qp_num,
			ses_verbs->rx_udp_port, packet_dump_str);
	}

	if (unlikely(test_management_session(ses_net)))
		goto deliver;
	
	ses_rpc = (struct session_rpc *)ses_net->transport_private;
	
	delay_ns = get_last_delay(ses_rpc);
	refill_send_credit(ses_rpc->send_state);
	refill_recv_credit();

	adjust_send_window(ses_rpc->send_state, delay_ns);

deliver:
	if (likely(zerocopy)) {
		z_buf[0] = recv_buf;
		z_buf_size[0] = recv_size;
	} else {
		memcpy(buf, recv_buf, recv_size);
	}
	return recv_size;
}


static inline int rpc_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return __rpc_receive_one(net,
				 buf, buf_size,
				 NULL, NULL,
				 false,		/* zerocopy */
				 false);	/* non_blocking */
}

static inline int rpc_receive_one_nb(struct session_net *net,
				     void *buf, size_t buf_size)
{
	return __rpc_receive_one(net,
				 buf, buf_size,
				 NULL, NULL,
				 false,		/* zerocopy */
				 true);		/* non_blocking */
}

static inline int rpc_receive_one_zerocopy(struct session_net *net,
				  void **buf, size_t *buf_size)
{
	return __rpc_receive_one(net,
				 NULL, 0,
				 buf, buf_size,
				 true,		/* zerocopy */
				 false);	/* non_blocking */
}

static inline int
rpc_receive_one_zerocopy_nb(struct session_net *net, void **buf, size_t *buf_size)
{
	return __rpc_receive_one(net,
				 NULL, 0,
				 buf, buf_size,
				 true,		/* zerocopy */
				 true);		/* non_blocking */
}

/*
 * Create a send state for a board, and link it to the per board send state linklist
 */
static int add_board_send_state(unsigned int board_ip,
				struct per_board_send_state **state)
{
	struct per_board_send_state *send_state;

	send_state = malloc(sizeof(struct per_board_send_state));
	if (!send_state)
		return -ENOMEM;

	send_state->board_ip = board_ip;
	atomic_init(&send_state->send_credit, default_credit_send);
	atomic_init(&send_state->session_cnt, 0);

	pthread_spin_lock(&global_cc_states.lock);
	list_add(&send_state->list, &global_cc_states.per_board_states);
	pthread_spin_unlock(&global_cc_states.lock);
	*state = send_state;

	return 0;
}

static int init_session_rpc(struct session_net *net, struct session_rpc *rpc)
{
	struct per_board_send_state *send_state;
	bool found;
	int ret;

	/*
	 * find if there are sessions using that board,
	 * if so, we will refer to the send state for that board
	 */
	found = false;
	list_for_each_entry (send_state, &global_cc_states.per_board_states,
			     list) {
		if (send_state->board_ip == net->board_ip) {
			found = true;
			break;
		}
	}

	/* else, create a send state for that board */
	if (!found) {
		ret = add_board_send_state(net->board_ip, &send_state);
		if (ret)
			return ret;
	}

	/* increase the reference count */
	atomic_fetch_add(&send_state->session_cnt, 1);
	rpc->send_state = send_state;
	atomic_init(&rpc->ring_head, 0);
	atomic_init(&rpc->ring_tail, 0);

	return 0;
}

static int
rpc_open_session(struct session_net *ses_net, struct endpoint_info *local_ei,
		 struct endpoint_info *remote_ei)
{
	struct session_rpc *ses_rpc;
	int ret;
	void *buf;
	struct msg_buf *mb;

	ses_rpc = malloc(sizeof(struct session_rpc));
	if (!ses_rpc)
		return -ENOMEM;
	ses_net->transport_private = ses_rpc;

	ret = init_session_rpc(ses_net, ses_rpc);
	if (ret) {
		free(ses_rpc);
		ses_net->transport_private = NULL;
		return ret;
	}

	buf = malloc(sysctl_link_mtu);
	if (!buf) {
		free(ses_rpc);
		ses_net->transport_private = NULL;
		return -ENOMEM;
	}

	mb = raw_net_reg_msg_buf(ses_net, buf, sysctl_link_mtu);
	if (!mb) {
		free(buf);
		free(ses_rpc);
		ses_net->transport_private = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int
rpc_close_session(struct session_net *ses_net)
{
	struct session_rpc *ses_rpc;

	ses_rpc = (struct session_rpc *)ses_net->transport_private;
	if (ses_rpc) {
		rpc_debug("Session %d\n", ses_net->session_id);
		struct per_board_send_state *send_state = ses_rpc->send_state;
		/* delete the state when no session is using that board */
		if (send_state && atomic_fetch_sub(&send_state->session_cnt, 1) == 1) {
			list_del(&send_state->list);
			free(send_state);
		}
		free(ses_rpc);
	}
	return 0;
}

static int rpc_init_once(struct endpoint_info *local_ei)
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

	atomic_init(&global_cc_states.recv_credit, default_credit_recv);
	INIT_LIST_HEAD(&global_cc_states.per_board_states);
	pthread_spin_init(&global_cc_states.lock, PTHREAD_PROCESS_PRIVATE);

	return 0;
}

static void rpc_exit(void)
{
	return;
}

struct transport_net_ops transport_rpc_ops = {
	.name			= "transport_rpc_cc",

	.init_once		= rpc_init_once,
	.exit			= rpc_exit,

	.open_session		= rpc_open_session,
	.close_session		= rpc_close_session,

	.send_one		= rpc_send_one,

	.receive_one		= rpc_receive_one,
	.receive_one_nb		= rpc_receive_one_nb,
	.receive_one_zerocopy	= rpc_receive_one_zerocopy,
	.receive_one_zerocopy_nb= rpc_receive_one_zerocopy_nb,

	.reg_send_buf		= default_transport_reg_send_buf,
};
