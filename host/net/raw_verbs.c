/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Transport layer over raw packet IB Verbs.
 * More specific, we use IBV_QPT_RAW_PACKET QPs.
 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>
#include <infiniband/verbs.h>

#include "net.h"
#include "../core.h"

/*
 * There is only one global QP and paired CQs for one running instance.
 * It works even if there are multiple instances on the same machine
 * (each using different udp ports of course).
 *
 * Parameters can be tuned:
 * 1. qp_init_attr, esp max_send_wr and max_recv_wr.
 */

/*
 * FIXME
 * The zerocopy trick that used by GBN works if and only if
 *   GBN window size > BUFFER_SIZE*NR_BUFFER_DEPTH
 *
 * Because the ib_raw layer never free any buffers, it simply reuse
 * the buffer by re-posting it to the RECV_CQ. On the other hand,
 * GBN layer simply enqueue the buffer into its temporary data list,
 * waiting apps to call receive_one.
 *
 * If ib_raw layer's ring buffer is smaller than the GBN window size,
 * IB device might override buffer still sitting in the temporary list!
 * Thus, we need to add such checks in the future.
 *
 * also, we need to take number of connections into account.
 */
#define BUFFER_SIZE	4096	/* maximum size of each send buffer */
#define NR_BUFFER_DEPTH	256	/* maximum number of sends waiting for completion */

int ib_port = 1;

/*
 * TODO
 * What's the best inline size? any past eval?
 * Check the atc16 paper.
 */
#define DEFAULT_MAX_INLINE_SIZE (128)

static struct session_raw_verbs cached_session_raw_verbs;
static pthread_spinlock_t raw_verbs_lock;

static int raw_verbs_reg_send_buf(struct session_net *ses_net,
				  void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_mr *mr;
	struct ibv_pd *pd;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	pd = ses_verbs->pd;

	if (unlikely(ses_verbs->send_mr)) {
		dprintf_ERROR("Send buf already registered for this session. "
		              "addr = %lx size = %zu. "
			      "It's possible to override but this feature is "
			      "not implemented for now.\n",
				(unsigned long)ses_verbs->send_buf,
				ses_verbs->send_buf_size);
		return -EEXIST;
	}

	mr = ibv_reg_mr(pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("reg mr:");
		return errno;
	}

	ses_verbs->send_mr = mr;
	ses_verbs->send_buf = buf;
	ses_verbs->send_buf_size = buf_size;
	return 0;
}

static struct msg_buf *
raw_verbs_reg_msg_buf(struct session_net *ses_net, void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct msg_buf *mb;
	struct ibv_mr *mr;
	struct ibv_pd *pd;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	if (!ses_verbs) {
		dprintf_ERROR("no ses_verbs installed %d\n", 0);
		return NULL;
	}
	pd = ses_verbs->pd;

	mr = ibv_reg_mr(pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("reg mr:");
		return NULL;
	}

	mb = malloc(sizeof(*mb));
	if (!mb) {
		ibv_dereg_mr(mr);
		return NULL;
	}

	mb->buf = buf;
	mb->max_buf_size = buf_size;
	mb->private = mr;
	return mb;
}

static int raw_verbs_dereg_msg_buf(struct session_net *net, struct msg_buf *mb)
{
	struct ibv_mr *mr;

	if (!mb)
		return -EINVAL;

	mr = (struct ibv_mr *)mb->private;
	ibv_dereg_mr(mr);
	free(mb);
	return 0;
}

static int
__raw_verbs_send(struct session_net *ses_net,
		 void *buf, size_t buf_size, struct ibv_mr *send_mr, void *_route)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_sge sge;
	struct ibv_send_wr wr, *bad_wr;
	struct ibv_qp *qp;
	struct ibv_cq *send_cq;
	bool signaled;
	int ret;
	struct routing_info *route;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	qp = ses_verbs->qp;
	send_cq = ses_verbs->send_cq;

	/*
	 * Cook the L2-L4 layer headers
	 * If users provide their own ri, we use it.
	 * Otherwise use the session ri.
	 */
	if (_route)
		route = (struct routing_info *)_route;
	else
		route = &ses_net->route;

	prepare_headers(route, buf, buf_size);

	sge.addr = (uint64_t)buf;
	sge.length = buf_size;
	sge.lkey = send_mr->lkey;

	wr.num_sge = 1;
	wr.sg_list = &sge;
	wr.next = NULL;
	wr.opcode = IBV_WR_SEND;

	wr.send_flags = 0;
	if (buf_size <= DEFAULT_MAX_INLINE_SIZE)
		wr.send_flags |= IBV_SEND_INLINE;

	/*
	 * TODO
	 * We could do batch signaling.
	 * There are probably two ways to implement that
	 * 1. have SIGNALED every N requests
	 * 2. have SIGNALED for every request, but poll every N requests
	 *
	 * We should aware that each CQE is a DMA-write from RNIC to DRAM.
	 * I know LegoOS's LITE is doing the second way, but I don't think
	 * that's the best way. Investigate more and come back optimize.
	 * eRPC's code is using the second way.
	 */
	if (1) {
		wr.send_flags |= IBV_SEND_SIGNALED;
		signaled = true;
	} else
		signaled = false;

	ret = ibv_post_send(qp, &wr, &bad_wr);
	if (unlikely(ret < 0)) {
		dprintf_ERROR("Fail to post send WQE %d\n", errno);
		goto out;
	}

	if (signaled) {
		while (1) {
			struct ibv_wc wc;

			ret = ibv_poll_cq(send_cq, 1, &wc);
			if (unlikely(!ret))
				continue;
			else if (unlikely(ret < 0)) {
				dprintf_ERROR("Fail to poll CQ %d\n", errno);
				goto out;
			}
			break;
		}
	}

	inc_stat(STAT_NET_RAW_VERBS_NR_TX);
	ret = buf_size;
out:
	return ret;
}

static int
raw_verbs_send_msg_buf(struct session_net *ses_net,
		       struct msg_buf *mb, size_t buf_size, void *route)
{
	struct ibv_mr *send_mr;
	void *buf;

	send_mr = (struct ibv_mr *)mb->private;
	buf = mb->buf;
	return __raw_verbs_send(ses_net, buf, buf_size, send_mr, route);
}

static int
raw_verbs_send(struct session_net *ses_net,
	       void *buf, size_t buf_size, void *route)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_mr *send_mr;
	bool new_mr;
	int ret;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;

	/*
	 * There are 3 cases
	 * 1. Buffer not registered.
	 * 2. Buffer registered, but caller is NOT using that.
	 * 3. Buffer registered, and caller is using that.
	 * Case 3 is the normal case.
	 */
	if (unlikely(!ses_verbs->send_mr)) {
		/*
		 * Case 1:
		 * Buffer not registered.
		 * We need to reg/dereg this buffer just of this send event.
		 * Caller should avoid this behavior on critical datapath.
		 */
		send_mr = ibv_reg_mr(ses_verbs->pd, buf, buf_size,
				     IBV_ACCESS_LOCAL_WRITE);
		if (!send_mr) {
			dprintf_ERROR("Fail to register memory. %d\n", errno);
			return errno;
		}
		new_mr = true;
	} else {
		new_mr = false;
		send_mr = ses_verbs->send_mr;

		if (unlikely(ses_verbs->send_buf_size < buf_size)) {
			dprintf_ERROR("Buffer too small. reg buf size: %zu, "
				      "new msg size: %zu\n",
				      ses_verbs->send_buf_size, buf_size);
			return -EINVAL;
		}

		/*
		 * Case 2:
		 * Caller used a different buffer
		 * and we do not need to anything for case 3
		 */
		if (unlikely(buf != ses_verbs->send_buf)) {
			dprintf_INFO("You have registered buffer but now "
				     "are using a different one. "
				     "There are perf penalties. (o %lx n %lx)\n",
				     (unsigned long)ses_verbs->send_buf,
				     (unsigned long)buf);
			memcpy(ses_verbs->send_buf, buf, buf_size);
			buf = ses_verbs->send_buf;
		}
	}

	ret = __raw_verbs_send(ses_net, buf, buf_size, send_mr, route);

	if (unlikely(new_mr))
		ibv_dereg_mr(send_mr);
	return ret;
}

/*
 * TODO
 *
 * This function can be optimized. Now we are doing individual post,
 * which is a single CPU-initiated MMIO write. If we chain all recv_wr
 * together, the driver will go for doorbell way, thus only one DMA read
 * from RNIC. Consider this function is sitting in data path, we should
 * optimize it. Reference: atc16, eRPC, and so on.
 */
static inline void post_recvs(struct session_raw_verbs *ses)
{
	struct ibv_sge sge;
	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	int i;

	sge.lkey = ses->recv_mr->lkey;
	sge.length = BUFFER_SIZE;
	recv_wr.num_sge = 1;
	recv_wr.sg_list = &sge;
	recv_wr.next = NULL;

	for (i = 0; i < NR_BUFFER_DEPTH; i++) {
		sge.addr = (uint64_t)(ses->recv_buf + BUFFER_SIZE * i);
		recv_wr.wr_id = i;
		if (ibv_post_recv(ses->qp, &recv_wr, &bad_recv_wr) < 0) {
			printf("Fail to post recv wr\n");
			exit(1);
		}
	}
	inc_stat(STAT_NET_RAW_VERBS_NR_POST_RECVS);
}

/*
 * Our current caller is gbn_poll_func, and it is single thread.
 * Thus this function basically is single-threaded.
 */
static int raw_verbs_receive_zerocopy(void **buf, size_t *buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	int ret;
	void *recv_buf;

	if (unlikely(!buf || !buf_size))
		return -EINVAL;

	ses_verbs = &cached_session_raw_verbs;
	recv_cq = ses_verbs->recv_cq;
	recv_buf = ses_verbs->recv_buf;

	while (1) {
		struct ibv_wc wc;
		void *buf_p;

		ret = ibv_poll_cq(recv_cq, 1, &wc);
		if (!ret)
			continue;
		else if (ret < 0) {
			perror("Poll CQ:");
			return ret;
		}
		inc_stat(STAT_NET_RAW_VERBS_NR_RX_ZEROCOPY);

		/* Get its position in the ring buffer */
		buf_p = recv_buf + wc.wr_id * BUFFER_SIZE;

		*buf = buf_p;
		*buf_size = wc.byte_len;

		if (unlikely(wc.wr_id == (NR_BUFFER_DEPTH - 1)))
			post_recvs(ses_verbs);
		break;
	}

	ret = *buf_size;
	return ret;
}

static int raw_verbs_receive(void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	void *recv_buf;
	int ret;

	ses_verbs = &cached_session_raw_verbs;
	recv_cq = ses_verbs->recv_cq;
	recv_buf = ses_verbs->recv_buf;

	while (1) {
		struct ibv_wc wc;
		void *buf_p;

		ret = ibv_poll_cq(recv_cq, 1, &wc);
		if (!ret)
			continue;
		else if (ret < 0) {
			perror("Poll CQ:");
			return ret;
		}
		inc_stat(STAT_NET_RAW_VERBS_NR_RX);

		/* Get its position in the ring buffer */
		buf_p = recv_buf + wc.wr_id * BUFFER_SIZE;

		if (unlikely(wc.byte_len > buf_size)) {
			dprintf_ERROR("Buf too small (received_size: %u "
				      "user_buf_size: %zu)\n",
				      wc.byte_len, buf_size);
			return -EIO;
		}

		/* Extra memcpy is needed if user provides buffer */
		buf_size = wc.byte_len;
		memcpy(buf, buf_p, buf_size);

		if (unlikely(wc.wr_id == (NR_BUFFER_DEPTH - 1)))
			post_recvs(ses_verbs);
		break;
	}

	ret = buf_size;
	return ret;
}

/*
 * Internal function to prepare receving buffers.
 * Allocate buffer, register, then post recvs.
 */
static void initial_post_recvs(struct session_raw_verbs *ses_verbs)
{
	void *recv_buf;
	int recv_buf_size;
	struct ibv_mr *recv_mr;

	recv_buf_size = BUFFER_SIZE * NR_BUFFER_DEPTH;
	recv_buf = malloc(recv_buf_size);
	if (!recv_buf) {
		printf("Fail to allocate memory\n");
		return;
	}
	memset(recv_buf, 0, recv_buf_size);

	recv_mr = ibv_reg_mr(ses_verbs->pd, recv_buf, recv_buf_size,
			     IBV_ACCESS_LOCAL_WRITE);
	if (!recv_mr) {
		printf("Coundn't register recv mr\n");
		return;
	}
	ses_verbs->recv_mr = recv_mr;
	ses_verbs->recv_buf = recv_buf;

	post_recvs(ses_verbs);
}

static int
raw_verbs_open_session(struct session_net *ses_net,
		       struct endpoint_info *local_ei,
		       struct endpoint_info *remote_ei)
{
	struct session_raw_verbs *ses_verbs;

	ses_verbs = malloc(sizeof(struct session_raw_verbs));
	if (!ses_verbs)
		return -ENOMEM;
	ses_net->raw_net_private = ses_verbs;

	/* Copy the whole thing */
	*ses_verbs = cached_session_raw_verbs;

	/*
	 * Make sure the per-session local variables
	 * are initiated right
	 */
	ses_verbs->send_mr = NULL;
	ses_verbs->send_buf = NULL;
	ses_verbs->send_buf_size = 0;
	return 0;
}

static int raw_verbs_close_session(struct session_net *ses_net)
{
	struct session_raw_verbs *ses_verbs;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	free(ses_verbs);
	return 0;
}

/*
 * This function install the flow control rules for @qp.
 * Once NIC can install multiple flows at the same time.
 *
 * It is a requirement to install this flow for Mellanox NIC.
 * The flow is more like a filter rather than a flow control method.
 */
static struct ibv_flow *
qp_create_flow(struct ibv_qp *qp, struct endpoint_info *local,
	       unsigned int qp_ib_port)
{
	struct raw_eth_flow_attr {
		struct ibv_flow_attr		attr;
		struct ibv_flow_spec_eth	spec_eth;
		struct ibv_flow_spec_ipv4	spec_ipv4;
		struct ibv_flow_spec_tcp_udp	spec_tcp_udp;
	} __attribute__((packed)) flow_attr;

	struct ibv_flow			*eth_flow;
	struct ibv_flow_attr		*attr;
	struct ibv_flow_spec_eth	*spec_eth;
	struct ibv_flow_spec_ipv4	*spec_ipv4;
	struct ibv_flow_spec_tcp_udp	*spec_tcp_udp;

	/* Shift to get all pointers */
	memset(&flow_attr, 0, sizeof(flow_attr));
	attr = &flow_attr.attr;
	spec_eth = &flow_attr.spec_eth;
	spec_ipv4 = &flow_attr.spec_ipv4;
	spec_tcp_udp = &flow_attr.spec_tcp_udp;

	/* Fill ibv_flow_attr */
	attr->comp_mask = 0;
	attr->type = IBV_FLOW_ATTR_NORMAL;
	attr->size = sizeof(flow_attr);
	attr->priority = 1;
	attr->num_of_specs = 3;
	attr->port = qp_ib_port;
	attr->flags = 0;

	/* Fill ibv_flow_spec_eth */
	spec_eth->type = IBV_FLOW_SPEC_ETH;
	spec_eth->size = sizeof(struct ibv_flow_spec_eth);
	memcpy(&spec_eth->val.dst_mac, local->mac, 6);
	memset(&spec_eth->mask.dst_mac, 0xFF, 6);

	/* IPv4 widecard */
	spec_ipv4->type = IBV_FLOW_SPEC_IPV4,
	spec_ipv4->size = sizeof(struct ibv_flow_spec_ipv4);

	/* Steer packets for this UDP port */
	spec_tcp_udp->type = IBV_FLOW_SPEC_UDP;
	spec_tcp_udp->size = sizeof(struct ibv_flow_spec_tcp_udp);
	spec_tcp_udp->val.dst_port = htons(local->udp_port);
	spec_tcp_udp->mask.dst_port = 0xFFFFu;

	eth_flow = ibv_create_flow(qp, &flow_attr.attr);
	if (!eth_flow)
		perror("Create_flow: ");
	return eth_flow;
}

/*
 * This function only run once at each host.
 * This function creates a single QP, Send_CQ, Recv_CQ,
 * and install flow control rules etc.
 */
static int raw_verbs_init_once(struct endpoint_info *local_ei)
{
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev;
	struct ibv_context *context;
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *cq, *recv_cq;
	struct ibv_flow *eth_flow;
	struct ibv_qp_attr qp_attr;
	int qp_flags;
	int ret;
	char ndev[32];

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get devices list");
		return -ENODEV;
	}

	ib_dev = dev_list[0];
	if (!ib_dev) {
		fprintf(stderr, "IB device not found\n");
		return -ENODEV;
	}

	ret = ibdev2netdev(ibv_get_device_name(ib_dev), ndev, sizeof(ndev));
	if (ret) {
		dprintf_ERROR("fail to do ibdev2netdev %d\n", 0);
		return ret;
	}

	if (strncmp(ndev, global_net_dev, sizeof(ndev))) {
		dprintf_ERROR("We are using ibdev [%s], which maps to network "
			      "device [%s]. But user passed device is [%s]. "
			      "If you wish to continue using raw_verbs, "
			      "please restart and use \"--dev=%s\"\n",
		ibv_get_device_name(ib_dev), ndev, global_net_dev, ndev);
		return -EIO;
	}
	dprintf_INFO("Using IB Device: %s (ndev: %s)\n",
		ibv_get_device_name(ib_dev), ndev);

	context = ibv_open_device(ib_dev);
	if (!context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		return -ENODEV;
	}

	pd = ibv_alloc_pd(context);
	if (!pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		return -ENOMEM;
	}

	/* Create send CQ */
	cq = ibv_create_cq(context, NR_BUFFER_DEPTH, NULL, NULL, 0);
	if (!cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		goto out_pd;
	}

	/* Create recv CQ */
	recv_cq = ibv_create_cq(context, NR_BUFFER_DEPTH, NULL, NULL, 0);
	if (!recv_cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		goto out_send_cq;
	}

	/*
	 * Create a raw_packet QP
	 * which uses the above send CQ and recv CQ
	 */
	struct ibv_qp_init_attr qp_init_attr = {
		.qp_context = NULL,
		.send_cq = cq,
		.recv_cq = recv_cq,
		.cap = {
			.max_send_wr = NR_BUFFER_DEPTH,
			.max_recv_wr = NR_BUFFER_DEPTH,
			.max_send_sge = 1,
			.max_recv_sge = 1, 
			.max_inline_data = DEFAULT_MAX_INLINE_SIZE,
		},

		/*
		 * This is the key difference.
		 * We are using RAW PACKET QPs.
		 */
		.qp_type = IBV_QPT_RAW_PACKET,
	};

	qp = ibv_create_qp(pd, &qp_init_attr);
	if (!qp) {
		fprintf(stderr, "Couldn't create RSS QP\n");
		goto out_both_cq;
	}

	/* QP: to INIT state */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE | IBV_QP_PORT;
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.port_num = ib_port;

	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "Failed modify qp to init\n");
		goto out_qp;
	}

	/* QP: to Ready-to-Receive state */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to receive\n");
		goto out_qp;
	}

	/* QP: to Ready-to-Send state */
	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to send\n");
		goto out_qp;
	}

	/*
	 * Install flow control rules
	 * This step is necessary for RAW_
	 */
	eth_flow = qp_create_flow(qp, local_ei, ib_port);
	if (!eth_flow)
		goto out_qp;

	cached_session_raw_verbs.eth_flow = eth_flow;
	cached_session_raw_verbs.pd = pd;
	cached_session_raw_verbs.qp = qp;
	cached_session_raw_verbs.send_cq = cq;
	cached_session_raw_verbs.recv_cq = recv_cq;
	cached_session_raw_verbs.lock = &raw_verbs_lock;
	pthread_spin_init(&raw_verbs_lock, PTHREAD_PROCESS_PRIVATE);

	initial_post_recvs(&cached_session_raw_verbs);

	return 0;

out_qp:
	ibv_destroy_qp(qp);
out_both_cq:
	ibv_destroy_cq(recv_cq);
out_send_cq:
	ibv_destroy_cq(cq);
out_pd:
	ibv_dealloc_pd(pd);
	return -1;
}

static void raw_verbs_exit(void)
{
	return;
}

struct raw_net_ops raw_verbs_ops = {
	.name			= "raw_verbs",
	.init_once		= raw_verbs_init_once,
	.exit			= raw_verbs_exit,

	.open_session		= raw_verbs_open_session,
	.close_session		= raw_verbs_close_session,

	.send_one		= raw_verbs_send,
	.send_one_msg_buf	= raw_verbs_send_msg_buf,
	.receive_one		= raw_verbs_receive,
	.receive_one_zerocopy	= raw_verbs_receive_zerocopy,
	.receive_one_nb		= NULL,

	.reg_send_buf		= raw_verbs_reg_send_buf,

	.reg_msg_buf		= raw_verbs_reg_msg_buf,
	.dereg_msg_buf		= raw_verbs_dereg_msg_buf,
};
