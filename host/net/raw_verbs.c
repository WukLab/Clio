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

#define PORT_NUM 1
#define BUFFER_SIZE	4096	/* maximum size of each send buffer */
#define SQ_NUM_DESC	512	/* maximum number of sends waiting for completion */

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
 * Cook the L2-L4 headers.
 * @len: the length of the whole packet.
 */
static __always_inline void prepare_headers(struct routing_info *route, void *buf, unsigned int len)
{
	struct routing_info *ri;

	memcpy(buf, route, sizeof(struct routing_info));

	ri = (struct routing_info *)buf;
	ri->ipv4.tot_len = htons(len - sizeof(struct eth_hdr));
	ri->ipv4.check = 0;
	ri->ipv4.check = ip_csum(&ri->ipv4, ri->ipv4.ihl);

	ri->udp.len = htons(len - sizeof(struct eth_hdr) - sizeof(struct ipv4_hdr));
}

/*
 * This function install the flow control rules for @qp.
 */
static struct ibv_flow *qp_create_flow(struct ibv_qp *qp, struct endpoint_info *local,
			  struct endpoint_info *remote)
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

	memset(&flow_attr, 0, sizeof(flow_attr));
	attr = &flow_attr.attr;
	spec_eth = &flow_attr.spec_eth;
	spec_ipv4 = &flow_attr.spec_ipv4;
	spec_tcp_udp = &flow_attr.spec_tcp_udp;

	attr->comp_mask = 0,
	attr->type = IBV_FLOW_ATTR_NORMAL,
	attr->size = sizeof(flow_attr),
	attr->priority = 1,
	attr->num_of_specs = 3,
	attr->port = 1,
	attr->flags = 0,

	spec_eth->type = IBV_FLOW_SPEC_ETH;
	spec_eth->size = sizeof(struct ibv_flow_spec_eth);
	memcpy(&spec_eth->val.dst_mac, local->mac, 6);
	memset(&spec_eth->mask.dst_mac, 0xFF, 6);

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

void dump_packet_headers(void *packet)
{
	struct eth_hdr *eth;
	struct ipv4_hdr *ipv4;
	struct udp_hdr *udp;
	int i, j;

	eth = packet;
	ipv4 = packet + sizeof(*eth);
	udp = packet + sizeof(*eth) + sizeof(*ipv4);

	for (j = 0; j < 6; j++) {
		printf("%x:", eth->src_mac[j]);
	}
	printf(" -> ");
	for (j = 0; j < 6; j++) {
		printf("%x:", eth->dst_mac[j]);
	}

	printf("  IP %x -> %x", ntohl(ipv4->src_ip), ntohl(ipv4->dst_ip));
	printf("  Port %u -> %u\n", ntohs(udp->src_port), ntohs(udp->dst_port));
}

/*
 * XXX
 * Instead of reg/dereg mr every time, we could use
 * a preallocated/register hugepage ring buffer.
 */
int raw_verbs_send(struct session_net *ses_net, void *buf, size_t buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_sge sge;
	struct ibv_send_wr wr, *bad_wr;
	struct ibv_mr *mr;
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *send_cq;
	int ret;

	ses_verbs = (struct session_raw_verbs *)ses_net->transport_private;
	pd = ses_verbs->pd;
	qp = ses_verbs->qp;
	send_cq = ses_verbs->send_cq;

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

	/* XXX We could do batch signalling */
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

	ret = 0;
out:
	ibv_dereg_mr(mr);
	return ret;
}

/*
 * RECVs were posted beforehand.
 * Current mode is very limited.
 */
int raw_verbs_receive(struct session_net *ses_net, void **buf, int *buf_size)
{
	struct session_raw_verbs *ses_verbs;
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *recv_cq;
	struct ibv_mr *recv_mr;
	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	struct ibv_sge sge;
	int ret;
	void *recv_buf;

	ses_verbs = (struct session_raw_verbs *)ses_net->transport_private;
	pd = ses_verbs->pd;
	qp = ses_verbs->qp;
	recv_cq = ses_verbs->recv_cq;
	recv_buf = ses_verbs->recv_buf;
	recv_mr = ses_verbs->recv_mr;

	sge.length = BUFFER_SIZE;
	sge.lkey = recv_mr->lkey;
	recv_wr.num_sge = 1;
	recv_wr.sg_list = &sge;
	recv_wr.next = NULL;

	while (1) {
		struct ibv_wc wc;
		void *buf_p;

		ret = ibv_poll_cq(recv_cq, 1, &wc);
		if (!ret)
			continue;
		else if (ret < 0) {
			perror("Poll CQ:");
			goto out;
		}

		buf_p = recv_buf + wc.wr_id * BUFFER_SIZE;

		*buf = buf_p;
		*buf_size = wc.byte_len;

		/*
		 * XXX
		 * Instead of posting everytime
		 * we could do batch post
		 */
		sge.addr = (uint64_t)buf_p;
		recv_wr.wr_id = wc.wr_id;
		ret = ibv_post_recv(qp, &recv_wr, &bad_recv_wr);
		if (ret < 0) {
			perror("post recv");
			goto out;
		}
		break;
	}

	ret = 0;
out:
	return ret;
}

/*
 * Prepare buffers
 */
static void post_recvs(struct session_raw_verbs *ses_verbs)
{
	int ret;
	void *recv_buf;
	int recv_buf_size;
	struct ibv_mr *recv_mr;
	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	struct ibv_wc wc;
	struct ibv_sge sg_entry;
	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_cq *recv_cq;
	int n;

	qp = ses_verbs->qp;
	pd = ses_verbs->pd;
	recv_cq = ses_verbs->recv_cq;

	recv_buf_size = BUFFER_SIZE * SQ_NUM_DESC;
	recv_buf = malloc(recv_buf_size);
	if (!recv_buf) {
		printf("Fail to allocate memory\n");
		return;
	}
	memset(recv_buf, 0, recv_buf_size);

	recv_mr = ibv_reg_mr(pd, recv_buf, recv_buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!recv_mr) {
		printf("Coundn't register recv mr\n");
		return;
	}

	ses_verbs->recv_mr = recv_mr;
	ses_verbs->recv_buf = recv_buf;

	sg_entry.length = BUFFER_SIZE;
	sg_entry.lkey = recv_mr->lkey;
	recv_wr.num_sge = 1;
	recv_wr.sg_list = &sg_entry;
	recv_wr.next = NULL;

	for (n = 0; n < SQ_NUM_DESC; n++) {
		sg_entry.addr = (uint64_t)(recv_buf + BUFFER_SIZE * n);
		recv_wr.wr_id = n;
		if (ibv_post_recv(qp, &recv_wr, &bad_recv_wr) < 0) {
			printf("Fail to post recv wr\n");
			exit(1);
		}
	}
}

struct session_net *init_ib_raw_packet(struct endpoint_info *local_ei,
				       struct endpoint_info *remote_ei)
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
	ses_net->transport_private = ses_verbs;

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

	cq = ibv_create_cq(context, SQ_NUM_DESC, NULL, NULL, 0);
	if (!cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		goto out_pd;
	}

	recv_cq = ibv_create_cq(context, SQ_NUM_DESC, NULL, NULL, 0);
	if (!recv_cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		goto out_send_cq;
	}

	struct ibv_qp_init_attr qp_init_attr = {
		.qp_context = NULL,
		.send_cq = cq,
		.recv_cq = recv_cq,
		.cap = {
			.max_send_wr = SQ_NUM_DESC,
			.max_recv_wr = SQ_NUM_DESC,
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
	qp_attr.port_num = 1;

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

	eth_flow = qp_create_flow(qp, local_ei, remote_ei);
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
	post_recvs(ses_verbs);

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

void test_ib_raw_packet(struct session_net *ses_net)
{
	struct session_raw_verbs *ses_verbs;
	void *send_buf, *recv_buf;
	int send_buf_size, recv_buf_size;
	int i;

	ses_verbs = (struct session_raw_verbs *)ses_net->transport_private;
	send_buf_size = 128;
	send_buf = malloc(send_buf_size);

	for (i = 0; i < 10; i++) {
		raw_verbs_send(ses_net, send_buf, send_buf_size);
		raw_verbs_receive(ses_net, &recv_buf, &recv_buf_size);
		dump_packet_headers(recv_buf);
	}
}
