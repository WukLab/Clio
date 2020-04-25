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

static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

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

	tb.rx = malloc(THPOOL_BUFFER_SIZE);
	tb.tx = malloc(THPOOL_BUFFER_SIZE);
	if (!tb.rx || !tb.rx)
		return -ENOMEM;

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

	/* Invoke handler locally */
	handle_new_node(&tb);

	free(tb.rx);
	free(tb.tx);
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
	size = legomem_query_stat_resp_size();
	set_tb_tx_size(tb, size);

	local_stat = default_local_bi->stat;
	memcpy(resp->stat, local_stat, NR_STAT_TYPES * sizeof(unsigned long));
	resp->nr_items = NR_STAT_TYPES;
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

	if (0) {
		dprintf_INFO("received opcode: %u (%s)\n",
			opcode, legomem_opcode_str(opcode));
	}

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
		if (1) {
			char err_msg[128];
			dump_packet_headers(tb->rx, err_msg);
			dprintf_ERROR("received unknown or un-implemented opcode: %u (%s) packet dump: \n"
				      "%s\n", opcode, legomem_opcode_str(opcode), err_msg);
		}
		set_tb_tx_size(tb, sizeof(struct legomem_common_headers));
		return;
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
}

static void *dispatcher(void *_unused)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	int ret;

	tb = thpool_buffer_map;
	tw = thpool_worker_map;

	ret = net_reg_send_buf(mgmt_session, tb->tx, THPOOL_BUFFER_SIZE);
	if (ret) {
		dprintf_ERROR("Fail to register TX buffer %d\n", ret);
		return NULL;
	}

	while (1) {
#if 1
		ret = net_receive_zerocopy_nb(mgmt_session, &tb->rx, &tb->rx_size);
		if (ret <= 0)
			continue;

		/* We only have one thread, thus inline handling */
		worker_handle_request_inline(tw, tb);
#endif
	}
	return NULL;
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

	/* Open the local mgmt session */
	ret = init_local_management_session();
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	/*
	 * Add a special localhost board_info
	 * and a special localhost session_net
	 */
	add_localhost_bi(&default_local_ei);
 
	/*
	 * Create a local session for remote monitor's mgmt session
	 * A special monitor board_info is added as well
	 */
	ret = init_monitor_session(ndev, monitor_addr, &default_local_ei);
	if (ret) {
		printf("Fail to init monitor session\n");
		exit(-1);
	}

	/*
	 * Now init the thpool stuff and create a new thread
	 * to handle the mgmt session traffic. 
	 */
	init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map,
			   default_thpool_buffer_alloc_cb);

	create_watchdog_thread();

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
		int cpu, node;

		getcpu(&cpu, &node);
		dprintf_INFO("\n**\n"
			     "**\n"
			     "** Start running test cases...\n"
			     "** (on cpu %d node %d)\n"
			     "**\n"
			     "**\n", cpu, node);
		if (board_addr_set) {
			//ret = test_legomem_board(board_addr);
			//ret = test_raw_net(board_addr);
		}

		//ret = test_rel_net_normal();
		//ret = test_rel_net_mgmt();
		//ret = test_legomem_alloc_free();
		//ret = test_legomem_context();
		//ret = test_legomem_session();
		ret = test_legomem_migration();
	}

	pthread_join(mgmt_session->thread, NULL);
	return 0;
}
