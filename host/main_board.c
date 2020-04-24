/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <getopt.h>
#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/hashtable.h>
#include <uapi/bitops.h>
#include <uapi/sched.h>
#include <uapi/thpool.h>
#include <uapi/opcode.h>
#include <uapi/net_header.h>

#include "core.h"
#include "board_emulator/core.h"

#define NR_THPOOL_WORKERS	1
#define NR_THPOOL_BUFFER	32

/*
 * Each thpool worker is described by struct thpool_worker,
 * and it is a standalone thread, running the generic handler only.
 * We can have one or multiple workers depends on config.
 *
 * The flow is:
 * a) Dispatcher allocate thpool buffer
 * b) Dispatcher receive network packet from FPGA
 * c) Dispacther find a worker, and delegate the request
 * d) The worker handles the req, send reply to FPGA, and free the thpool buffer.
 *
 * The thpool buffer is using a simple ring-based design.
 */
static int TW_HEAD = 0;
static int TB_HEAD = 0;
static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

/*
 * Select a worker to handle the next request
 * in a round-robin fashion.
 */
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

static void worker_handle_request(struct thpool_worker *tw,
				  struct thpool_buffer *tb)
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

	/* VM */
	case OP_REQ_ALLOC:
		board_soc_handle_alloc_free(tb, true);
		break;
	case OP_REQ_FREE:
		board_soc_handle_alloc_free(tb, false);
		break;

	case OP_REQ_PINGPONG:
		handle_pingpong(tb);
		break;
	default:
		dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
			      opcode, legomem_opcode_str(opcode));
		set_tb_tx_size(tb, sizeof(struct legomem_common_headers));
		break;
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
	free_thpool_buffer(tb);
}

/*
 * Dispatcher uses local mgmt session to receive and send back msg.
 * Since mgmt session does not have any remote end's information,
 * we must rely on each packet's routing info to send it back.
 */
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
		ret = net_receive_zerocopy(mgmt_session, &tb->rx, &tb->rx_size);
		if (ret <= 0)
			continue;
#else
		ret = net_receive(mgmt_session, tb->rx, THPOOL_BUFFER_SIZE);
		if (ret <= 0)
			continue;
		tb->rx_size = ret;
#endif

		/*
		 * Inline handling for now
		 * We will need to pass down the request once go SMP
		 */
		worker_handle_request(tw, tb);
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
	req.op.type = BOARD_INFO_FLAGS_BOARD;
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
	printf("Usage ./board_emulator.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --monitor=<ip:port>         Specify monitor addr in IP:Port format (Required)\n"
	       "  --dev=<name>                Specify the local network device (Required)\n"
	       "  --port=<port>               Specify the local UDP port we listen to (Required)\n"
	       "  --net_raw_ops=[options]     Select the raw network layer implementation (Optional)\n"
	       "                              Available Options are:\n"
	       "                                1. raw_verbs (default if nothing is specified)\n"
	       "                                2. raw_udp\n"
	       "                                3. raw_socket\n"
	       "\n"
	       "Examples:\n"
	       "  ./board_emulator.o --port 8888 --dev=\"lo\" \n"
	       "  ./board_emulator.o -p 8888 -d ens4\n");
}

static struct option long_options[] = {
	{ "monitor",		required_argument,	NULL,	'm'},
	{ "port",		required_argument,	NULL,	'p'},
	{ "dev",		required_argument,	NULL,	'd'},
	{ "net_raw_ops",	required_argument,	NULL,	'n'},
	{ 0,			0,			0,	0  }
};

int main(int argc, char **argv)
{
	int ret;
	int c, option_index = 0;
	char monitor_addr[32];
	bool monitor_addr_set = false;
	char ndev[32];
	bool ndev_set = false;
	int port = 0;

	/* Parse arguments */
	while (1) {
		c = getopt_long(argc, argv, "m:p:d:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			strncpy(monitor_addr, optarg, sizeof(monitor_addr));
			monitor_addr_set = true;
			break;
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

	/* Same as host side init */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

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

	join_cluster();
	pthread_join(mgmt_session->thread, NULL);
	return 0;
}
