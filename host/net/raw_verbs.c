/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Transport layer over raw packet IB Verbs.
 * More specific, we use IBV_QPT_RAW_PACKET QPs.
 */
#include <netinet/in.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>
#include <infiniband/verbs.h>

#include "net.h"
#include "../core.h"

#if 0
# define CONFIG_RAW_VERBS_DUMP_TX
#endif

int ib_port = 1;
struct ibv_context *ib_context;
struct ibv_pd *ib_pd;

static atomic_int base_udp_port;

/*
 * TODO
 * What's the best inline size? any past eval?
 * Check the atc16 paper.
 */
#define DEFAULT_MAX_INLINE_SIZE (128)

static int raw_verbs_reg_send_buf(struct session_net *ses_net,
				  void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_mr *mr;
	struct ibv_pd *pd;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	pd = ses_verbs->pd;

	if (ses_verbs->send_mr) {
		/*
		 * We don't free the buffer.
		 * Since we don't know if it could be freed.
		 */
		dprintf_DEBUG("NOTICE Overriding session %d registered send buf\n",
			get_local_session_id(ses_net));
		ibv_dereg_mr(ses_verbs->send_mr);
	}

	mr = ibv_reg_mr(pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("reg mr:");
		return errno;
	}

	dprintf_DEBUG("Registered buf: [%#lx - %#lx] len=%#lx session id %d\n",
			(u64)buf, (u64)buf + buf_size, (u64)buf_size,
			get_local_session_id(ses_net));

	ses_verbs->send_mr = mr;
	ses_verbs->send_buf = buf;
	ses_verbs->send_buf_size = buf_size;
	return 0;
}

struct ibv_mr *raw_verbs_reg_mr(void *buf, size_t buf_size)
{
	struct ibv_mr *mr;

	mr = ibv_reg_mr(ib_pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("reg mr:");
		return NULL;
	}
	return mr;
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
	 * Otherwise use the saved session ri.
	 */
	if (unlikely(_route))
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
	 * RDMAmojo:
	 * https://www.rdmamojo.com/2014/06/30/working-unsignaled-completions/
	 * All posted Send Requested, Signaled and Unsignaled,
	 * are considered outstanding until a Work Completion that they,
	 * or Send Requests that were posted after them, was polled from
	 * the Completion Queue associated with the Send Queue. 
	 *
	 * We must occasionally post a Send Request that generates Work Completion.
	 */
	if (!(ses_verbs->nr_post_send++ % NR_MAX_OUTSTANDING_SEND_WR)) {
		wr.send_flags |= IBV_SEND_SIGNALED;
		signaled = true;
	} else
		signaled = false;

#ifdef CONFIG_RAW_VERBS_DUMP_TX
	char packet_dump_str[256];
	dump_packet_headers(buf, packet_dump_str);
	dprintf_INFO("\033[32m TX signaled=%d QPN=%u pkt: %s size %zu \033[0m\n",
		signaled, qp->qp_num, packet_dump_str, buf_size);
#endif

	ret = ibv_post_send(qp, &wr, &bad_wr);
	if (unlikely(ret < 0)) {
		dprintf_ERROR("Fail to post send WQE %d\n", errno);
		goto out;
	}

	if (unlikely(signaled)) {
		while (1) {
			ret = ibv_poll_cq(send_cq, NR_MAX_OUTSTANDING_SEND_WR,
					  ses_verbs->send_wc);
			if (ret < 0) {
				dprintf_ERROR("Fail to poll CQ %d\n", errno);
				goto out;
			} else if (ret == 0)
				continue;
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

		dprintf_INFO("  Created a new MR for send. Check if this is on datapath!! %d\n", 0);
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
		if (unlikely(buf < ses_verbs->send_buf ||
			     buf > (ses_verbs->send_buf + ses_verbs->send_buf_size))) {
			dprintf_INFO("You have registered buffer but now "
				     "are using a different one. "
				     "There are perf penalties. (range [%lx-%#lx] current: %lx) "
				     "Session local_id=%u remote_id=%u\n",
				     (u64)ses_verbs->send_buf,
				     (u64)ses_verbs->send_buf + ses_verbs->send_buf_size,
				     (unsigned long)buf,
				     get_local_session_id(ses_net),
				     get_remote_session_id(ses_net));
			memcpy(ses_verbs->send_buf, buf, buf_size);
			buf = ses_verbs->send_buf;
		}
	}

	ret = __raw_verbs_send(ses_net, buf, buf_size, send_mr, route);

	if (unlikely(new_mr))
		ibv_dereg_mr(send_mr);
	return ret;
}

static void batched_post_recv(struct session_raw_verbs *ses, int nr_recvs)
{
	struct ibv_recv_wr *first_wr, *last_wr, *tmp_wr, *bad_wr;
	int first_wr_i, last_wr_i;
	int ret;

	ses->nr_delayed_recvs += nr_recvs;
	if (ses->nr_delayed_recvs < NR_BATCH_POST_RECV)
		return;

	first_wr_i = ses->recv_head;
	last_wr_i = first_wr_i + (ses->nr_delayed_recvs - 1);
	if (last_wr_i >= NR_BUFFER_DEPTH)
		last_wr_i -= NR_BUFFER_DEPTH;

	first_wr = &ses->recv_wr[first_wr_i];
	last_wr = &ses->recv_wr[last_wr_i];
	tmp_wr = last_wr->next;

	/* Break circularity for posting */
	last_wr->next = NULL;

	ret = ibv_post_recv(ses->qp, first_wr, &bad_wr);
	if (unlikely(ret < 0)) {
		dprintf_ERROR("Fail to post a new recv_wr %d\n", errno);
		dump_stats();
		dump_legomem_contexts();
		dump_net_sessions();
	}

	/* Restore circularity */
	last_wr->next = tmp_wr;

	ses->recv_head = (last_wr_i + 1) % NR_BUFFER_DEPTH;
	ses->nr_delayed_recvs = 0;
}

/*
 * Our current caller is gbn_poll_func, and it is single thread.
 * Thus this function basically is single-threaded.
 */
static int raw_verbs_receive_zerocopy_batch(struct session_net *ses_net,
					    void **buf, size_t *buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	int i, ret;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	recv_cq = ses_verbs->recv_cq;

	ret = ibv_poll_cq(recv_cq, NR_MAX_RECV_BATCH, ses_verbs->recv_wc);
	if (unlikely(ret < 0)) {
		perror("Poll CQ:");
		return ret;
	} else if (ret == 0)
		return 0;

	for (i = 0; i < ret; i++) {
		buf[i] = (void *)ses_verbs->recv_wc[i].wr_id;
		buf_size[i] = ses_verbs->recv_wc[i].byte_len;
	}

	batched_post_recv(ses_verbs, ret);
	return ret;
}

/*
 * Non-blocking, return 0 if there is no packet.
 */
static int raw_verbs_receive_zerocopy(struct session_net *ses_net,
				      void **buf, size_t *buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	struct ibv_wc wc;
	int ret;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	recv_cq = ses_verbs->recv_cq;

	ret = ibv_poll_cq(recv_cq, 1, &wc);
	if (unlikely(ret < 0)) {
		perror("Poll CQ:");
		return ret;
	} else if (ret == 0)
		return 0;

	buf[0] = (void *)wc.wr_id;
	buf_size[0] = wc.byte_len;

	batched_post_recv(ses_verbs, 1);
	return 1;
}

static int raw_verbs_receive(struct session_net *ses_net, void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	struct ibv_wc wc;
	int ret;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	recv_cq = ses_verbs->recv_cq;

	while (1) {
		void *buf_p;

		ret = ibv_poll_cq(recv_cq, 1, &wc);
		if (unlikely(ret < 0)) {
			perror("Poll CQ:");
			return ret;
		} else if (ret == 0)
			return 0;
		inc_stat(STAT_NET_RAW_VERBS_NR_RX);

		if (unlikely(wc.byte_len > buf_size)) {
			dprintf_ERROR("Buf too small (received_size: %u "
				      "user_buf_size: %zu)\n",
				      wc.byte_len, buf_size);
			return -EIO;
		}
		buf_size = wc.byte_len;

		/* Extra memcpy is needed if user provides buffer */
		buf_p = (void *)wc.wr_id;
		memcpy(buf, buf_p, buf_size);

		batched_post_recv(ses_verbs, 1);
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
	int i;
	u64 addr;
	struct ibv_recv_wr *bad_recv_wr;
	struct ibv_sge *recv_sge;
	struct ibv_recv_wr *recv_wr;

	recv_buf_size = BUFFER_SIZE * NR_BUFFER_DEPTH;

	recv_buf = mmap(0, recv_buf_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB,
			0, 0);
	if (recv_buf == MAP_FAILED) {
		perror("mmap");
		dprintf_ERROR("Fail to allocate memory %d\n", errno);
		exit(0);
	}

	recv_mr = ibv_reg_mr(ses_verbs->pd, recv_buf, recv_buf_size,
			     IBV_ACCESS_LOCAL_WRITE);
	if (!recv_mr) {
		printf("Coundn't register recv mr\n");
		return;
	}

	/* Save them */
	ses_verbs->recv_mr = recv_mr;
	ses_verbs->recv_buf = recv_buf;

	recv_wr = ses_verbs->recv_wr;
	recv_sge = ses_verbs->recv_sge;

	for (i = 0; i < NR_BUFFER_DEPTH; i++) {
		addr = (u64)(recv_buf + BUFFER_SIZE * i);

		recv_sge[i].addr = addr;
		recv_sge[i].lkey = recv_mr->lkey;
		recv_sge[i].length = BUFFER_SIZE;

		recv_wr[i].wr_id = addr;
		recv_wr[i].num_sge = 1;
		recv_wr[i].sg_list = &recv_sge[i];

		/* Circular link */
		if (i < (NR_BUFFER_DEPTH - 1))
			recv_wr[i].next = &recv_wr[i + 1];
		else
			recv_wr[i].next = &recv_wr[0];
	}

	/* Break the last one circular link for posting */
	recv_wr[NR_BUFFER_DEPTH - 1].next = NULL;
	if (ibv_post_recv(ses_verbs->qp, &recv_wr[0], &bad_recv_wr) < 0) {
		printf("Fail to post recv wr\n");
		exit(1);
	}

	/* Restore cirularity */
	recv_wr[NR_BUFFER_DEPTH - 1].next = &recv_wr[0];
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
	       unsigned int qp_ib_port, unsigned int rx_udp_port)
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
	spec_tcp_udp->val.dst_port = htons(rx_udp_port);
	spec_tcp_udp->mask.dst_port = 0xFFFFu;

	eth_flow = ibv_create_flow(qp, &flow_attr.attr);
	if (!eth_flow)
		perror("Create_flow: ");
	return eth_flow;
}

static int
raw_verbs_open_session(struct session_net *ses_net,
		       struct endpoint_info *local_ei,
		       struct endpoint_info *remote_ei)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_qp *qp;
	struct ibv_qp_attr qp_attr;
	int qp_flags, ret;
	struct ibv_cq *send_cq, *recv_cq;
	struct ibv_flow *eth_flow __maybe_unused;
	struct ibv_qp_init_attr qp_init_attr = {
		.qp_context = NULL,
		.cap = {
			.max_send_wr = NR_BUFFER_DEPTH,
			.max_recv_wr = NR_BUFFER_DEPTH,
			.max_send_sge = 2,
			.max_recv_sge = 2, 
			.max_inline_data = DEFAULT_MAX_INLINE_SIZE,
		},

		/*
		 * This is the key difference.
		 * We are using RAW PACKET QPs.
		 */
		.qp_type = IBV_QPT_RAW_PACKET,
		.sq_sig_all = 0
	};
	unsigned int rx_udp_port;

	ses_verbs = malloc(sizeof(struct session_raw_verbs));
	if (!ses_verbs)
		return -ENOMEM;
	memset(ses_verbs, 0, sizeof(*ses_verbs));
	ses_net->raw_net_private = ses_verbs;

	send_cq = ibv_create_cq(ib_context, NR_BUFFER_DEPTH, NULL, NULL, 0);
	if (!send_cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		return -ENOMEM;
	}

	/* Create recv CQ */
	recv_cq = ibv_create_cq(ib_context, NR_BUFFER_DEPTH, NULL, NULL, 0);
	if (!recv_cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		return -ENOMEM;
	}

	/*
	 * Use per-session send CQ
	 * But share a global rece CQ
	 */
	qp_init_attr.send_cq = send_cq;
	qp_init_attr.recv_cq = recv_cq;

	qp = ibv_create_qp(ib_pd, &qp_init_attr);
	if (!qp) {
		dprintf_ERROR("Fail to open a new QP %d\n", errno);
		return -ENOMEM;
	}

	/* QP: to INIT state */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE | IBV_QP_PORT;
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.port_num = ib_port;

	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "Failed modify qp to init\n");
		return -EPERM;
	}

	/* QP: to Ready-to-Receive state */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to receive\n");
		return -EPERM;
	}

	/* QP: to Ready-to-Send state */
	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to send\n");
		return -EPERM;
	}

	/*
	 * Each QP has its own unique RX UDP port
	 * Starting from base_udp_port.
	 */
	rx_udp_port = atomic_fetch_add(&base_udp_port, 1);

	ses_verbs->rx_udp_port = rx_udp_port;
	eth_flow = qp_create_flow(qp, &default_local_ei, ib_port, rx_udp_port);
	if (!eth_flow) {
		return -EPERM;
	}

	ses_verbs->pd = ib_pd;
	ses_verbs->qp = qp;
	ses_verbs->send_cq = send_cq;
	ses_verbs->recv_cq = recv_cq;
	ses_verbs->nr_post_send = 0;

	ses_verbs->recv_head = 0;
	ses_verbs->nr_delayed_recvs = 0;
	initial_post_recvs(ses_verbs);

	dprintf_CRIT("New QP/UDP pair created: QPN=%u rx_udp_port=%d\n", qp->qp_num, rx_udp_port);
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
 * This function only run once at each host.
 * This function creates a single QP, Send_CQ, Recv_CQ,
 * and install flow control rules etc.
 */
static int raw_verbs_init_once(struct endpoint_info *local_ei)
{
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev;
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

	ib_context = ibv_open_device(ib_dev);
	if (!ib_context) {
		fprintf(stderr, "Couldn't get ib_context for %s\n",
			ibv_get_device_name(ib_dev));
		return -ENODEV;
	}

	ib_pd = ibv_alloc_pd(ib_context);
	if (!ib_pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		return -ENOMEM;
	}

	atomic_store(&base_udp_port, local_ei->udp_port);
	return 0;
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

	.receive_one			= raw_verbs_receive,
	.receive_one_zerocopy		= raw_verbs_receive_zerocopy,
	.receive_one_zerocopy_batch	= raw_verbs_receive_zerocopy_batch,
	.receive_one_nb			= NULL,

	.reg_send_buf		= raw_verbs_reg_send_buf,

	.reg_msg_buf		= raw_verbs_reg_msg_buf,
	.dereg_msg_buf		= raw_verbs_dereg_msg_buf,
};
