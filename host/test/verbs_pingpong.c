#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>

#define PORT_NUM 1
#define ENTRY_SIZE	4096 /* maximum size of each send buffer */
#define SQ_NUM_DESC	512 /* maximum number of sends waiting for completion */

/*
 * This is a pre-computed per-endpoint L2-L4 headers.
 * It will be directly copied to the head of each packet.
 */
struct routing_info board_ri;

struct dummy_payload {
	unsigned long mark;
};

static inline void prepare_headers(void *buf, unsigned int len)
{
	struct routing_info *ri;

	memcpy(buf, &board_ri, sizeof(struct routing_info));

	ri = (struct routing_info *)buf;
	ri->ipv4.tot_len = htons(len - sizeof(struct eth_hdr));
	ri->ipv4.check = 0;
	ri->ipv4.check = ip_csum(&ri->ipv4, ri->ipv4.ihl);

	ri->udp.len = htons(len - sizeof(struct eth_hdr) - sizeof(struct ipv4_hdr));
}

struct ibv_flow *eth_flow = NULL;

/*
 * This function install the flow control rules for @qp.
 */
static int qp_create_flow(struct ibv_qp *qp, struct endpoint_info *local, struct endpoint_info *remote)
{
	struct raw_eth_flow_attr {
		struct ibv_flow_attr		attr;
		struct ibv_flow_spec_eth	spec_eth;
		struct ibv_flow_spec_ipv4	spec_ipv4;
		struct ibv_flow_spec_tcp_udp	spec_tcp_udp;
	} __attribute__((packed)) flow_attr;

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
	if (!eth_flow) {
		perror("Create_flow: ");
		exit(1);
		return -1;
	}
	return 0;
}

struct create_thread_info {
	struct ibv_qp *qp;
	struct ibv_cq *recv_cq;
	struct ibv_mr *recv_mr;
	char *recv_buf;
	int cpu;
};

pthread_t poll_thread_tid;

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
 * This thread will keep polling the recv_cq.
 */
static void *poll_thread_func(void *arg)
{
	struct create_thread_info *info = (struct create_thread_info *)arg;
	struct ibv_qp *qp = info->qp;
	struct ibv_cq *recv_cq = info->recv_cq;
	struct ibv_mr *recv_mr = info->recv_mr;
	char *recv_buf = info->recv_buf;

	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	struct ibv_wc wc;
	struct ibv_sge sg_entry;
	int ret, nr_rx = 0;

	sg_entry.length = ENTRY_SIZE;
	sg_entry.lkey = recv_mr->lkey;

	recv_wr.num_sge = 1;
	recv_wr.sg_list = &sg_entry;
	recv_wr.next = NULL;

	while (1) {
		void *packet;

		ret = ibv_poll_cq(recv_cq, 1, &wc);
		if (ret <= 0) {
			if (unlikely(ret < 0)) {
				printf("Poll CQ error\n");
				exit(1);
			}
			continue;
		}

		packet = recv_buf + wc.wr_id * ENTRY_SIZE;

		printf("RX wr_id=%ld size=%d mark=%d nr_rx = %d ",
			wc.wr_id, wc.byte_len,
			((struct dummy_payload *)(packet + 44))->mark, nr_rx);

		dump_packet_headers(packet);

		/* Reuse the slot */
		memset(packet, 0, ENTRY_SIZE);
		sg_entry.addr = (uint64_t)recv_buf + wc.wr_id * ENTRY_SIZE;
		recv_wr.wr_id = wc.wr_id;

		ret = ibv_post_recv(qp, &recv_wr, &bad_recv_wr);
		if (unlikely(ret < 0)) {
			printf("Post recv error\n");
			exit(1);
		}
		nr_rx++;
	}
}

#if 1
# define SEP_POLL_THREAD
#endif

/*
 * Post initial RECVs to the recv_cq
 * and create a new polling thread
 */
static struct create_thread_info *
prepare_polling_thread(struct ibv_pd *pd, struct ibv_qp *qp, struct ibv_cq *recv_cq)
{
	struct create_thread_info *info;
	int ret;
	void *recv_buf;
	int recv_buf_size = ENTRY_SIZE * SQ_NUM_DESC;
	struct ibv_mr *recv_mr;
	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	struct ibv_wc wc;
	struct ibv_sge sg_entry;
	int n;

	recv_buf = malloc(recv_buf_size);
	if (!recv_buf) {
		printf("Fail to allocate memory\n");
		return NULL;
	}
	memset(recv_buf, 0, recv_buf_size);

	recv_mr = ibv_reg_mr(pd, recv_buf, recv_buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!recv_mr) {
		printf("Coundn't register recv mr\n");
		return NULL;
	}

	/* Prepare the initial RECVs */
	sg_entry.length = ENTRY_SIZE;
	sg_entry.lkey = recv_mr->lkey;
	recv_wr.num_sge = 1;
	recv_wr.sg_list = &sg_entry;
	recv_wr.next = NULL;
	for (n = 0; n < SQ_NUM_DESC; n++) {
		sg_entry.addr = (uint64_t)recv_buf + ENTRY_SIZE * n;
		recv_wr.wr_id = n;
		if (ibv_post_recv(qp, &recv_wr, &bad_recv_wr) < 0) {
			printf("Fail to post recv wr\n");
			exit(1);
		}
	}

	info = malloc(sizeof(*info));
	if (!info)
		return NULL;

	info->qp = qp;
	info->recv_cq = recv_cq;
	info->recv_mr = recv_mr;
	info->recv_buf = recv_buf;
	info->cpu = 0;

#ifdef SEP_POLL_THREAD
	ret = pthread_create(&poll_thread_tid, NULL, poll_thread_func, info);
	if (ret) {
		printf("Fail to create poll thread\n");
		return NULL;
	}
#endif
	return info;
}

int main(void)
{
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev;
	struct ibv_context *context;
	struct ibv_pd *pd;
	int ret;

	if (sizeof(struct routing_info) != 42) {
		printf("mismatch\n");
		exit(-1);
	}
	printf("IP header size: %zu\n", sizeof(struct ipv4_hdr));

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get devices list");
		exit(1);
	}

	ib_dev = dev_list[0];
	if (!ib_dev) {
		fprintf(stderr, "IB device not found\n");
		exit(1);
	}

	printf("IB Device: %s\n", ibv_get_device_name(ib_dev));
	printf("Netdev Device: %s\n", ibv_get_device_name(ib_dev));

	context = ibv_open_device(ib_dev);
	if (!context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		exit(1);
	}

	pd = ibv_alloc_pd(context);
	if (!pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		exit(1);
	}

	/* 4. Create Complition Queue (CQ) */
	struct ibv_cq *cq, *recv_cq;
	cq = ibv_create_cq(context, SQ_NUM_DESC, NULL, NULL, 0);
	if (!cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		exit(1);
	}

	recv_cq = ibv_create_cq(context, SQ_NUM_DESC, NULL, NULL, 0);
	if (!recv_cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
		exit(1);
	}

	/* 5. Initialize QP */
	struct ibv_qp *qp;
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
		.qp_type = IBV_QPT_RAW_PACKET,
	};

	qp = ibv_create_qp(pd, &qp_init_attr);
	if (!qp) {
		fprintf(stderr, "Couldn't create RSS QP\n");
		exit(1);
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
		exit(1);
	}

	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to receive\n");
		exit(1);
	}

	qp_flags = IBV_QP_STATE;
	qp_attr.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
	if (ret < 0) {
		fprintf(stderr, "failed modify qp to send\n");
		exit(1);
	}

	/* Setup Send buffer */
	int n = 0;
	struct ibv_sge sg_entry;
	int buf_size = ENTRY_SIZE * SQ_NUM_DESC;
	void *send_buf;
	struct ibv_mr *mr;
	send_buf = malloc(buf_size);
	if (!send_buf) {
		fprintf(stderr, "Coudln't allocate memory\n");
		exit(1);
	}
	mr = ibv_reg_mr(pd, send_buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		fprintf(stderr, "Couldn't register mr\n");
		exit(1);
	}

	struct endpoint_info ei_wuklab02 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
		.ip		= 0xc0a80102, /* 192.168.1.2 */
		.udp_port	= 8888,
	};
	struct endpoint_info board_0 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
		.ip		= 0xc0a801c8, /* 192.168.1.200 */
		.udp_port	= 1234,
	};

	/* XXX more automatic or use config file */
	struct endpoint_info *local_ei, *remote_ei;
	local_ei	= &ei_wuklab02;
	remote_ei	= &board_0;

	qp_create_flow(qp, local_ei, remote_ei);

	prepare_routing_info(&board_ri, local_ei, remote_ei);

	struct create_thread_info *info;
	info = prepare_polling_thread(pd, qp, recv_cq);

#ifndef SEP_POLL_THREAD
	struct ibv_mr *recv_mr = info->recv_mr;
	char *recv_buf = info->recv_buf;
	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	struct ibv_wc recv_wc;
	struct ibv_sge recv_sg_entry;

	recv_sg_entry.length = ENTRY_SIZE;
	recv_sg_entry.lkey = recv_mr->lkey;

	recv_wr.num_sge = 1;
	recv_wr.sg_list = &recv_sg_entry;
	recv_wr.next = NULL;
#endif

	/* For latency measure */
	unsigned long *lat;
	unsigned long diff;
	struct timespec ts_s, ts_e;
	int nr_tests;

	nr_tests = 100;
	lat = malloc(nr_tests * sizeof(unsigned long));
	if (!lat)
		exit(1);
	memset(lat, 0, nr_tests * sizeof(unsigned long));

	int msgs_completed;
	struct ibv_wc wc;
	struct ibv_send_wr wr, *bad_wr;
	unsigned int pktlen = 128;

	/* TEST */
	n = 0;
	while (1) {
		void *buf = send_buf + ENTRY_SIZE * (n % SQ_NUM_DESC);
		struct dummy_payload *dp;

		prepare_headers(buf, pktlen);

		sg_entry.addr = (uint64_t)buf;
		sg_entry.length = pktlen;
		sg_entry.lkey = mr->lkey;

		memset(&wr, 0, sizeof(wr));
		wr.wr_id = n;
		wr.num_sge = 1;
		wr.sg_list = &sg_entry;
		wr.next = NULL;
		wr.opcode = IBV_WR_SEND;
		wr.send_flags = IBV_SEND_INLINE;
#define SEND_SIGNAL
#ifdef SEND_SIGNAL
		wr.send_flags |= IBV_SEND_SIGNALED;
#endif

		dp = buf + 44;
		dp->mark = n;

		usleep(1000);
		clock_gettime(CLOCK_MONOTONIC, &ts_s);
		ret = ibv_post_send(qp, &wr, &bad_wr);
		if (ret < 0) {
			fprintf(stderr, "failed in post send\n");
			exit(1);
		}

#ifdef SEND_SIGNAL
		while ((msgs_completed = ibv_poll_cq(cq, 1, &wc)) != 0) {
			if (msgs_completed > 0) {
				printf("completed message %ld\n", wc.wr_id);
			} else if (msgs_completed < 0) {
				printf("Polling error\n");
				exit(1);
			}
		}
#endif

#ifndef SEP_POLL_THREAD
		while ((ret = ibv_poll_cq(recv_cq, 1, &recv_wc)) <= 0) {
			if (unlikely(ret < 0)) {
				printf("Poll RECV CQ error\n");
				exit(1);
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &ts_e);
		diff = (ts_e.tv_sec * 1000000000 + ts_e.tv_nsec) - (ts_s.tv_sec * 1000000000 + ts_s.tv_nsec);

		printf("Received msg wr_id=%ld size %d RTT: %lu mark: %lu\n",
			recv_wc.wr_id, recv_wc.byte_len, diff,
			((struct dummy_payload *)(recv_buf + recv_wc.wr_id * ENTRY_SIZE + 44))->mark);

		/* Reuse the slot */
		recv_sg_entry.addr = (uint64_t)recv_buf + recv_wc.wr_id * ENTRY_SIZE;
		recv_wr.wr_id = recv_wc.wr_id;

		ret = ibv_post_recv(qp, &recv_wr, &bad_recv_wr);
		if (unlikely(ret < 0)) {
			printf("Post recv error\n");
			exit(1);
		}
#endif

		n++;
		if (n >= nr_tests)
			break;
	}

	double total_ns = 0;
	for (n = 0; n < nr_tests; n++)
		total_ns += lat[n];

	printf("Avg: %#lf ns\n", (double)total_ns / (double)nr_tests);

	if (eth_flow)
		ibv_destroy_flow(eth_flow);

#ifdef SEP_POLL_THREAD
	pthread_join(poll_thread_tid, NULL);
#endif
	return 0;
}
