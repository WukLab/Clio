/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/thpool.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "core.h"
#include "net/net.h"

/*
 * We only need to a single worker thus inline handling
 */
#define NR_THPOOL_WORKERS	1
#define NR_THPOOL_BUFFER	1

static int TW_HEAD = 0;
static int TB_HEAD = 0;
static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

static __always_inline struct thpool_worker *
select_thpool_worker_rr(void)
{
	struct thpool_worker *tw;
	int idx;

	idx = TW_HEAD % NR_THPOOL_WORKERS;
	tw = thpool_worker_map + idx;
	TW_HEAD++;
	return tw;
}

static __always_inline struct thpool_buffer *
alloc_thpool_buffer(void)
{
	struct thpool_buffer *tb;
	int idx;

	idx = TB_HEAD % NR_THPOOL_BUFFER;
	tb = thpool_buffer_map + idx;
	TB_HEAD++;

	/*
	 * If this happens during runtime, it means:
	 * - ring buffer is not large enough
	 * - some previous handlers are too slow
	 */
	while (unlikely(ThpoolBufferUsed(tb))) {
		;
	}

	SetThpoolBufferUsed(tb);
	barrier();
	return tb;
}

static __always_inline void
free_thpool_buffer(struct thpool_buffer *tb)
{
	tb->flags = 0;
	barrier();
}

static void handle_close_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	unsigned int ip, port;
	char ip_str[INET_ADDRSTRLEN];
	struct board_info *bi;
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;
	struct session_net *ses_net;
	unsigned int dst_sesid;
	int ret;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	ipv4_hdr = to_ipv4_header(req);
	udp_hdr = to_udp_header(req);

	ip = ntohl(ipv4_hdr->src_ip);
	get_ip_str(ip, ip_str);
	port = ntohs(udp_hdr->src_port);

	bi = find_board(ip, port);
	if (!bi) {
		dprintf_ERROR("board not found %s:%d\n", ip_str, port);
		goto error;
	}

	/* Find if the session exist */
	dst_sesid = req->op.session_id;
	ses_net = find_net_session(ip, port, dst_sesid);
	if (!ses_net) {
		dprintf_ERROR("session not found %s:%d sesid %u\n",
			ip_str, port, dst_sesid);
		dump_net_sessions();
		goto error;
	}

	ret = generic_handle_close_session(NULL, bi, ses_net);
	if (ret) {
		dprintf_ERROR("fail to close session %s:%d sesid %u\n",
			ip_str, port, dst_sesid);
		dump_net_sessions();
		goto error;
	}

	dprintf_DEBUG("session closed, remote: %s remote sesid: %u local sesid: %u\n",
		bi->name, get_remote_session_id(ses_net), get_local_session_id(ses_net));

	/* Success */
	resp->op.session_id = 0;
	return;

error:
	resp->op.session_id = -EFAULT;
}

/*
 * Handle the case when a remote party wants to open a session
 * with us. It must tell us its local session id.
 */
static void handle_open_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	unsigned int ip, port;
	struct board_info *bi;
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;
	struct session_net *ses_net;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	ipv4_hdr = to_ipv4_header(req);
	udp_hdr = to_udp_header(req);

	ip = ntohl(ipv4_hdr->src_ip);
	port = ntohs(udp_hdr->src_port);

	bi = find_board(ip, port);
	if (!bi) {
		char ip_str[INET_ADDRSTRLEN];
		get_ip_str(ip, ip_str);
		dprintf_ERROR("board not found %s:%d\n", ip_str, port);
		dump_boards();
		goto error;
	}

	ses_net = generic_handle_open_session(bi, req->op.session_id);
	if (!ses_net) {
		dprintf_ERROR("fail to open receiver side session. sender: %s\n",
			bi->name);
		goto error;
	}

	resp->op.session_id = get_local_session_id(ses_net);

	dprintf_DEBUG("session opened, remote: %s remote sesid: %u local sesid: %u\n",
		bi->name, get_remote_session_id(ses_net), get_local_session_id(ses_net));

	return;

error:
	resp->op.session_id = 0;
}

/*
 * Handle the case when _monitor_ notifies us
 * that there is a new node joining the cluster.
 * We will add it to our local list.
 */
static void handle_new_node(struct thpool_buffer *tb)
{
	struct legomem_membership_new_node_req *req;
	struct legomem_membership_new_node_resp *resp;
	struct endpoint_info *new_ei;
	struct board_info *bi;
	int ret, i;
	unsigned char mac[6];
	int ip;
	char *ip_str;

	/* Setup response */
	resp = (struct legomem_membership_new_node_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
	resp->ret = 0;

	/* Setup request */
	req = (struct legomem_membership_new_node_req *)tb->rx;
	new_ei = &req->op.ei;

	/* Sanity check */
	if (req->op.type != BOARD_INFO_FLAGS_HOST &&
	    req->op.type != BOARD_INFO_FLAGS_BOARD) {
		dprintf_ERROR("invalid type: %lu %s\n",
			req->op.type, board_info_type_str(req->op.type));
		return;
	}

	/*
	 * We may use a different local MAC address to reach the new host
	 * run our local ARP protocol to get the latest and update if necessary.
	 */
	ip = new_ei->ip;
	ip_str = (char *)new_ei->ip_str;
	ret = get_mac_of_remote_ip(ip, ip_str, global_net_dev, mac);
	if (ret) {
		dprintf_ERROR("fail to get mac of new node. ip %s\n", ip_str);
		return;
	}

	if (memcmp(mac, new_ei->mac, 6)) {
		printf("%s(): INFO mac updated ", __func__);
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", new_ei->mac[i]);
			else
				printf("%x -> ", new_ei->mac[i]);
		}
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", mac[i]);
			else
				printf("%x\n", mac[i]);
		}

		/* Override the original MAC */
		memcpy(new_ei->mac, mac, 6);
	}

	/* Finally add the board to the system */
	bi = add_board(req->op.name, req->op.mem_size_bytes,
		       new_ei, &default_local_ei, false);
	if (!bi)
		return;
	bi->flags = req->op.type;

	dprintf_INFO("new node added name: %s, ip:port: %s:%d type: %s\n",
		req->op.name, new_ei->ip_str, new_ei->udp_port,
		board_info_type_str(bi->flags));

	dump_boards();
	dump_net_sessions();
}

/*
 * Manually add a new remote node.
 * Since this is not broadcast from monitor, this information is local only.
 * This interface is mainly designed for testing purpose.
 */
int manually_add_new_node(unsigned int ip, unsigned int udp_port,
			  unsigned int node_type)
{
	struct thpool_buffer tb;
	struct legomem_membership_new_node_req *req;
	struct endpoint_info *ei;
	char ip_str[INET_ADDRSTRLEN];
	char new_name[BOARD_NAME_LEN];

	/* Cook the name */
	get_ip_str(ip, ip_str);
	if (node_type == BOARD_INFO_FLAGS_BOARD) {
		sprintf(new_name, "t_board_%s:%u", ip_str, udp_port);
	} else {
		dprintf_ERROR("Manual adding only supports _board_ for now. (%s)\n",
			board_info_type_str(node_type));
		return -EINVAL;
	}

	req = (struct legomem_membership_new_node_req *)tb.rx;

	/*
	 * We do not need to fill the mac addr
	 * let the handler figure it out
	 */
	ei = &req->op.ei;
	memcpy(ei->ip_str, ip_str, INET_ADDRSTRLEN);
	ei->ip = ip;
	ei->udp_port = udp_port;

	/* Fill in the fake request */
	req->op.type = node_type;
	req->op.mem_size_bytes = 0;
	memcpy(req->op.name, new_name, BOARD_NAME_LEN);

	handle_new_node(&tb);
	return 0;
}

int manually_add_new_node_str(const char *ip_port_str, unsigned int node_type)
{
	int ip, port;
	int ip1, ip2, ip3, ip4;

	sscanf(ip_port_str, "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;
	return manually_add_new_node(ip, port, node_type);
}

static void handle_query_stat(struct thpool_buffer *tb)
{
	struct legomem_query_stat_resp *resp;
	size_t size;
	unsigned long *local_stat;

	/*
	 * calculate the size of resp msg
	 * minus 1 because of the original pointer
	 */
	resp = (struct legomem_query_stat_resp *)tb->tx;
	size = sizeof(*resp) + (NR_STAT_TYPES - 1) * sizeof(unsigned long);
	set_tb_tx_size(tb, size);

	local_stat = default_local_bi->stat;
	memcpy(resp->stat, local_stat, NR_STAT_TYPES * sizeof(unsigned long));
	resp->nr_items = NR_STAT_TYPES;
}

static void handle_pingpong(struct thpool_buffer *tb)
{
	struct legomem_pingpong_resp *resp;

	resp = (struct legomem_pingpong_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
}

static void
worker_handle_request_inline(struct thpool_worker *tw, struct thpool_buffer *tb)
{
	struct lego_header *lego_hdr;
	struct gbn_header *gbn_hdr;
	uint16_t opcode;
	struct routing_info *ri;

	lego_hdr = to_lego_header(tb->rx);
	opcode = lego_hdr->opcode;

	switch (opcode) {
	case OP_REQ_MEMBERSHIP_NEW_NODE:
		handle_new_node(tb);
		break;
	case OP_OPEN_SESSION:
		handle_open_session(tb);
		break;
	case OP_CLOSE_SESSION:
		handle_close_session(tb);
		break;
	case OP_REQ_PINGPONG:
		handle_pingpong(tb);
		break;
	case OP_REQ_QUERY_STAT:
		handle_query_stat(tb);
		break;
	default:
		dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
			opcode, legomem_opcode_str(opcode));
		goto free;
	};

	if (likely(!ThpoolBufferNoreply(tb))) {
		/*
		 * We the mgmt session accepting all traffics
		 * thus we do not really know who is the sender prior
		 * we can only infer that info from the incoming traffic
		 * To reply, we can only, and should, simply swap the routing info
		 */
		ri = (struct routing_info *)tb->rx;
		swap_routing_info(ri);

		/*
		 * Original must be X -> 0
		 * It will become 0 -> X
		 * (X is larger than 0)
		 */
		gbn_hdr = to_gbn_header(tb->rx);
		swap_gbn_session(gbn_hdr);

		net_send_with_route(mgmt_session, tb->tx, tb->tx_size, ri);
	}

free:
	free_thpool_buffer(tb);
}

static void *dispatcher(void *_unused)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	int ret;

	while (1) {
		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		ret = net_receive(mgmt_session, tb->rx, THPOOL_BUFFER_SIZE);
		if (ret <= 0)
			continue;
		tb->rx_size = ret;

		/* We only have one thread, thus inline handling */
		worker_handle_request_inline(tw, tb);
	}
	return NULL;
}
/*
 * Use input @monitor_addr to create a local network session with the monitor.
 * Note we just create local data structures for monitor's management session.
 * We do not need to contact monitor for this particular creation.
 */
static int init_monitor_session(char *ndev, char *monitor_addr,
				struct endpoint_info *local_ei)
{
	char ip_str[INET_ADDRSTRLEN];
	int ip, port;
	int ip1, ip2, ip3, ip4;
	struct in_addr in_addr;
	unsigned char mac[6];
	struct endpoint_info monitor_ei;
	int ret, i;

	sscanf(monitor_addr, "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	in_addr.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in_addr, ip_str, sizeof(ip_str));

	ret = get_mac_of_remote_ip(ip, ip_str, ndev, mac);
	if (ret) {
		dprintf_ERROR("cannot get mac of ip %s\n", ip_str);
		return ret;
	}

	/*
	 * Now let's save the info
	 * into the global variables
	 */
	memcpy(monitor_ip_str, ip_str, INET_ADDRSTRLEN);
	monitor_ip_h = ip;

	/* EI needs host order ip */
	monitor_ei.ip = ip;
	monitor_ei.udp_port = port;
	memcpy(monitor_ei.mac, mac, 6);

	monitor_bi = add_board("monitor", 0,
			       &monitor_ei, local_ei,
			       false);
	if (!monitor_bi)
		return -ENOMEM;

	monitor_bi->flags |= BOARD_INFO_FLAGS_MONITOR;

	monitor_session = get_board_mgmt_session(monitor_bi);

	/* Debugging info */
	printf("%s(): monitor IP: %s Port: %d MAC: ",
		__func__, monitor_ip_str, port);
	for (i = 0; i < 6; i++)
		printf("%x ", mac[i]);
	printf("\n");
	return 0;
}

void test(void)
{
	struct msg {
		struct legomem_common_headers comm_headers;
		int cnt;
	} req;
	struct lego_header *lego_header;
	int i;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_TEST;

	for (i = 0; i < 100; i++) {
		req.cnt = i;
		net_send(monitor_session, &req, sizeof(req));
		printf("%s(): finished sending cnt %5d\n", __func__, req.cnt);
		//net_receive(monitor_session, &resp, sizeof(resp));
		//printf("%s(): finished receiving cnt %5d\n", __func__, req.cnt);
	}
}

static int join_cluster(void)
{
	struct legomem_membership_join_cluster_req req;
	struct legomem_membership_join_cluster_resp resp;
	struct lego_header *lego_header;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_MEMBERSHIP_JOIN_CLUSTER;

	req.op.mem_size_bytes = 0;
	req.op.type = BOARD_INFO_FLAGS_HOST;
	req.op.ei = default_local_ei;

	ret = net_send_and_receive(monitor_session, &req, sizeof(req),
				   &resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_ERROR("net error %d\n", ret);
		return -1;
	}

	if (resp.ret == 0)
		dprintf_INFO("Succefully joined cluster! (monitor: %s)\n",
			monitor_bi->name);
	else
		dprintf_ERROR("Fail to join cluster, monitor error %d\n",
			resp.ret);
	return resp.ret;
}

static void print_usage(void)
{
	printf("Usage ./host.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --monitor=<ip:port>         Specify monitor addr in IP:Port format (Required)\n"
	       "  --skip_join                 Do not contact monitor for cluster join\n"
	       "                              This is useful if there is no real monitor running (Optional)\n"
	       "  --dev=<name>                Specify local network device (Required)\n"
	       "  --port=<port>               Specify local UDP port (Required)\n"
	       "  --net_raw_ops=[options]     Select raw network layer implementation (Optional)\n"
	       "                              Available Options are:\n"
	       "                                1. raw_verbs (default if nothing is specified)\n"
	       "                                2. raw_udp\n"
	       "                                3. raw_socket\n"
	       "  --net_trans_ops=[options]   Select transport layer implementations (Optional)\n"
	       "                              Available Options are:\n"
	       "                                1. gbn (go-back-N reliable stack, default if nothing is specified)\n"
	       "                                2. bypass (simple bypass transport layer, unreliable)\n"
	       "  --add_board=<ip:port>       Manually add a remote board (Optional)\n"
	       "  --run_test                  Run built-in tests (Optional)"
	       "\n"
	       "Examples:\n"
	       "  ./host.o --monitor=\"127.0.0.1:8888\" --port 8887 --dev=\"lo\" \n"
	       "  ./host.o -m 127.0.0.1:8888 -p 8887 -d lo\n");
}

#define OPT_NET_TRANS_OPS		(10000)
static struct option long_options[] = {
	{ "monitor",		required_argument,	NULL,	'm'},
	{ "skip_join",  	no_argument,		NULL,	's'},
	{ "port",		required_argument,	NULL,	'p'},
	{ "dev",		required_argument,	NULL,	'd'},
	{ "net_raw_ops",	required_argument,	NULL,	'n'},
	{ "net_trans_ops",	required_argument,	NULL,	OPT_NET_TRANS_OPS},
	{ "add_board",		required_argument,	NULL,	'b'},
	{ "run_test",		no_argument,		NULL,	't'},
	{ 0,			0,			0,	0  }
};

int main(int argc, char **argv)
{
	int ret;
	int c, option_index = 0;
	char monitor_addr[32];
	bool monitor_addr_set = false;
	bool skip_join_set = false;
	char ndev[32];
	bool ndev_set = false;
	int port = 0;
	bool run_test = false;

	char board_addr[32];
	char board_addr_set = false;

	/* Parse arguments */
	while (1) {
		c = getopt_long(argc, argv, "m:p:d:b:t",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			strncpy(monitor_addr, optarg, sizeof(monitor_addr));
			monitor_addr_set = true;
			break;
		case 's': {
			skip_join_set = true;
			break;
		}
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			strncpy(ndev, optarg, sizeof(ndev));
			strncpy(global_net_dev, optarg, sizeof(global_net_dev));
			ndev_set = true;
			break;
		case 'n':
			if (!strncmp(optarg, "raw_verbs", 16))
				raw_net_ops = &raw_verbs_ops;
			else if (!strncmp(optarg, "raw_udp", 16))
				raw_net_ops = &raw_udp_socket_ops;
			else if (!strncmp(optarg, "raw_socket", 16))
				raw_net_ops = &raw_socket_ops;
			else {
				printf("Invalid net_raw_ops: %s\n"
				       "Available Options are:\n"
				       "  1. raw_verbs (default if nothing is specified)\n"
				       "  2. raw_udp\n"
				       "  3. raw_socket\n", optarg);
				exit(-1);
			}
			break;
		case OPT_NET_TRANS_OPS:
			if (!strncmp(optarg, "gbn", 8))
				transport_net_ops = &transport_gbn_ops;
			else if (!strncmp(optarg, "bypass", 8))
				transport_net_ops = &transport_bypass_ops;
			else {
				printf("Invalid net_trans_ops: %s\n"
				       "Available Options are:\n"
				       "1. gbn (go-back-N reliable stack, default if nothing is specified)\n"
				       "2. bypass (simple bypass transport layer, unreliable)\n", optarg);
				exit(-1);
			}
			break;
		case 'b':
			strncpy(board_addr, optarg, sizeof(board_addr));
			board_addr_set = true;
			break;
		case 't':
			run_test = true;
			break;
		default:
			print_usage();
			exit(-1);
		}
	}

	/* Check if required arguments are passed */
	if (!ndev_set) {
		printf("ERROR: Please specify the network device (Use ifconfig to check).\n\n");
		print_usage();
		return 0;
	}

	if (!monitor_addr_set) {
		printf("ERROR: Please specify the monitor address\n\n");
		print_usage();
		return 0;
	}
	printf("monitor: %s ndev: %s port: %d\n", monitor_addr, ndev, port);

	/*
	 * Init the local endpoint info
	 * - mac, ip, port
	 * Use information based on ndev and port.
	 */
	ret = init_default_local_ei(ndev, port, &default_local_ei);
	if (ret) {
		printf("Fail to init local endpoint. ndev %s port %d\n",
			ndev, port);
		exit(-1);
	}

	ret = init_net(&default_local_ei);
	if (ret) {
		printf("Fail to init network layer.\n");
		exit(-1);
	}

	/*
	 * Mainly init spinlocks
	 * This must take place before any init operations
	 * invovling boards, session, etc.
	 */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	/*
	 * Create a local session for remote monitor's mgmt session
	 * A special monitor board_info is added as well
	 */
	ret = init_monitor_session(ndev, monitor_addr, &default_local_ei);
	if (ret) {
		printf("Fail to init monitor session\n");
		exit(-1);
	}

	/* Add the special localhost board_info */
	add_localhost_bi(&default_local_ei);
 
	inc_stat(STAT_NET_NR_RX);
	dump_stats();

	/* Open the local mgmt session */
	ret = init_local_management_session();
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	/*
	 * Now init the thpool stuff and create a new thread
	 * to handle the mgmt session traffic. 
	 */
	init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map,
			   default_thpool_buffer_alloc_cb);

	ret = pthread_create(&mgmt_session->thread, NULL, dispatcher, NULL);
	if (ret) {
		dprintf_ERROR("Fail to create mgmt thread %d\n", errno);
		exit(-1);
	}

	/*
	 * We only contact monitor if the --skip_join flag is NOT passed.
	 * This special flag is for testing purpose.
	 */
	if (likely(!skip_join_set)) {
		ret = join_cluster();
		if (ret)
			exit(-1);
	} else {
		dprintf_INFO("Skipped join_cluster, monitor (%s) is not contacted.\n",
			monitor_bi->name);
	}

	/*
	 * Manually add some remote boards, do this after we've joined cluster.
	 * This special flag is more testing purpose too, before our board
	 * has a equivalant version of join_cluster.
	 */
	if (board_addr_set) {
		manually_add_new_node_str(board_addr, BOARD_INFO_FLAGS_BOARD);
	}

	/*
	 * Run predefined testing if there is any.
	 */
	if (run_test) {
		if (board_addr_set)
			ret = test_legomem_board(board_addr);

		//ret = test_raw_net();
		//ret = test_legomem_session();
		//ret = test_legomem_migration();
	}

	pthread_join(mgmt_session->thread, NULL);
	return 0;
}
