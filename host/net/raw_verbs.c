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

#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>
#include <infiniband/verbs.h>

#include "net.h"

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
#define NR_BUFFER_DEPTH	512	/* maximum number of sends waiting for completion */

int ib_port = 1;

struct session_raw_verbs {
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *send_cq;

	struct ibv_cq *recv_cq;
	struct ibv_mr *recv_mr;
	void *recv_buf;

	struct ibv_flow *eth_flow;
};

/*
 * This function install the flow control rules for @qp.
 */
static struct ibv_flow *qp_create_flow(struct ibv_qp *qp, struct endpoint_info *local,
			  struct endpoint_info *remote, unsigned int qp_ib_port)
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

	if (!qp || !local || !remote)
		return NULL;

	if (!qp_ib_port)
		return NULL;

	memset(&flow_attr, 0, sizeof(flow_attr));
	attr = &flow_attr.attr;
	spec_eth = &flow_attr.spec_eth;
	spec_ipv4 = &flow_attr.spec_ipv4;
	spec_tcp_udp = &flow_attr.spec_tcp_udp;

	attr->comp_mask = 0;
	attr->type = IBV_FLOW_ATTR_NORMAL;
	attr->size = sizeof(flow_attr);
	attr->priority = 1;
	attr->num_of_specs = 3;
	attr->port = qp_ib_port;
	attr->flags = 0;

	spec_eth->type = IBV_FLOW_SPEC_ETH;
	spec_eth->size = sizeof(struct ibv_flow_spec_eth);
	memcpy(&spec_eth->val.dst_mac, local->mac, 6);
	memset(&spec_eth->mask.dst_mac, 0xFF, 6);

	/*
	 * TODO Not sure if we need to have this
	 * Enable it will lead to segfault
	 */
	//memcpy(&spec_eth->val.src_mac, remote->mac, 6);
	//memset(&spec_eth->mask.src_mac, 0xFF, 6);

	spec_ipv4->type = IBV_FLOW_SPEC_IPV4,
	spec_ipv4->size = sizeof(struct ibv_flow_spec_ipv4);

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
 * TODO
 * 1) Instead of reg/dereg mr every time, we could use
 *    a preallocated/register hugepage ring buffer.
 * 2) We could do batch signaling.
 * 3) Add timeout to poll_cq
 */
static int raw_verbs_send(struct session_net *ses_net, void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_sge sge;
	struct ibv_send_wr wr, *bad_wr;
	struct ibv_mr *mr;
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *send_cq;
	int ret;

	if (unlikely(!ses_net || !buf || buf_size > sysctl_link_mtu))
		return -EINVAL;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
	pd = ses_verbs->pd;
	qp = ses_verbs->qp;
	send_cq = ses_verbs->send_cq;

	if (unlikely(!ses_verbs || !pd || !qp || !send_cq))
		return -EINVAL;

	mr = ibv_reg_mr(pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("reg mr:");
		return errno;
	}

	prepare_headers(&ses_net->route, buf, buf_size);

	sge.addr = (uint64_t)buf;
	sge.length = buf_size;
	sge.lkey = mr->lkey;

	memset(&wr, 0, sizeof(wr));
	wr.wr_id = 0;
	wr.num_sge = 1;
	wr.sg_list = &sge;
	wr.next = NULL;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = IBV_SEND_INLINE;

	/* TODO We could do batch signalling */
	wr.send_flags |= IBV_SEND_SIGNALED;

	ret = ibv_post_send(qp, &wr, &bad_wr);
	if (ret < 0) {
		perror("Post Send:");
		goto out;
	}

	while (1) {
		struct ibv_wc wc;

		ret = ibv_poll_cq(send_cq, 1, &wc);
		if (!ret)
			continue;
		else if (ret < 0) {
			perror("poll cq:");
			goto out;
		}

		/* Finished */
		break;
	}

	ret = buf_size;
out:
	ibv_dereg_mr(mr);
	return ret;
}

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
}

static int raw_verbs_receive_zerocopy(struct session_net *ses_net,
				      void **buf, size_t *buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	int ret;
	void *recv_buf;

	if (unlikely(!ses_net || !buf || !buf_size))
		return -EINVAL;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
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

static int raw_verbs_receive(struct session_net *ses_net,
			     void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_cq *recv_cq;
	void *recv_buf;
	int ret;

	if (unlikely(!ses_net || !buf || buf_size > sysctl_link_mtu))
		return -EINVAL;

	ses_verbs = (struct session_raw_verbs *)ses_net->raw_net_private;
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

		/* Get its position in the ring buffer */
		buf_p = recv_buf + wc.wr_id * BUFFER_SIZE;

		if (unlikely(wc.byte_len > buf_size)) {
			printf("%s(): buf too small (%u %zu)\n",
				__func__, wc.byte_len, buf_size);
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

void test_ib_raw_packet(struct session_net *ses_net)
{
	void *send_buf, *recv_buf;
	size_t send_buf_size, recv_buf_size;
	int i;

	send_buf_size = 128;
	recv_buf_size = 128;
	send_buf = malloc(send_buf_size);
	recv_buf = malloc(recv_buf_size);

	for (i = 0; i < 10; i++) {
		raw_verbs_send(ses_net, send_buf, send_buf_size);
		raw_verbs_receive(ses_net, recv_buf, recv_buf_size);
		dump_packet_headers(recv_buf);
	}
}

static struct session_net *
init_raw_verbs(struct endpoint_info *local_ei, struct endpoint_info *remote_ei)
{
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev;
	struct ibv_context *context;
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *cq, *recv_cq;
	struct ibv_flow *eth_flow;
	struct session_net *ses_net;
	struct session_raw_verbs *ses_verbs;
	int ret;

	ses_net = malloc(sizeof(struct session_net));
	if (!ses_net)
		return NULL;

	ses_verbs = malloc(sizeof(struct session_raw_verbs));
	if (!ses_verbs) {
		free(ses_net);
		return NULL;
	}
	ses_net->raw_net_private = ses_verbs;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get devices list");
		goto free_session;
	}

	ib_dev = dev_list[0];
	if (!ib_dev) {
		fprintf(stderr, "IB device not found\n");
		goto free_session;
	}

	printf("IB Device: %s\n", ibv_get_device_name(ib_dev));
	printf("Netdev Device: %s\n", ibv_get_device_name(ib_dev));

	context = ibv_open_device(ib_dev);
	if (!context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		goto free_session;
	}

	pd = ibv_alloc_pd(context);
	if (!pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto free_session;
	}

	cq = ibv_create_cq(context, NR_BUFFER_DEPTH, NULL, NULL, 0);
	if (!cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		goto out_pd;
	}

	recv_cq = ibv_create_cq(context, NR_BUFFER_DEPTH, NULL, NULL, 0);
	if (!recv_cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		goto out_send_cq;
	}

	struct ibv_qp_init_attr qp_init_attr = {
		.qp_context = NULL,
		.send_cq = cq,
		.recv_cq = recv_cq,
		.cap = {
			.max_send_wr = NR_BUFFER_DEPTH,
			.max_recv_wr = NR_BUFFER_DEPTH,
			.max_send_sge = 1,
			.max_recv_sge = 1, 
			.max_inline_data = 512,
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

	struct ibv_qp_attr qp_attr;
	int qp_flags;

	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE | IBV_QP_PORT;
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.port_num = ib_port;

	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "Failed modify qp to init\n");
		goto out_qp;
	}

	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to receive\n");
		goto out_qp;
	}

	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to send\n");
		goto out_qp;
	}

	eth_flow = qp_create_flow(qp, local_ei, remote_ei, ib_port);
	if (!eth_flow)
		goto out_qp;

	/* Prepare the session info */
	prepare_routing_info(&ses_net->route, local_ei, remote_ei);
	memcpy(&ses_net->local_ei, local_ei, sizeof(*local_ei));
	memcpy(&ses_net->remote_ei, remote_ei, sizeof(*remote_ei));

	ses_verbs->eth_flow = eth_flow;
	ses_verbs->pd = pd;
	ses_verbs->qp = qp;
	ses_verbs->send_cq = cq;
	ses_verbs->recv_cq = recv_cq;

	initial_post_recvs(ses_verbs);

	return ses_net;

out_qp:
	ibv_destroy_qp(qp);
out_both_cq:
	ibv_destroy_cq(recv_cq);
out_send_cq:
	ibv_destroy_cq(cq);
out_pd:
	ibv_dealloc_pd(pd);
free_session:
	free(ses_net);
	free(ses_verbs);
	return NULL;
}

struct raw_net_ops raw_verbs_ops = {
	.name			= "raw_verbs",
	.send_one		= raw_verbs_send,
	.receive_one		= raw_verbs_receive,
	.receive_one_zerocopy	= raw_verbs_receive_zerocopy,
	.receive_one_nb		= NULL,
	.init			= init_raw_verbs,
};
