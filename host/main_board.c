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

static void handle_test(struct thpool_buffer *tb)
{
	struct reply {
		struct legomem_common_headers comm_headers;
		int cnt;
	} *reply, *req;
	static int nr_rx = 0;

	reply = (struct reply *)tb->tx;
	set_tb_tx_size(tb, sizeof(*reply));

	nr_rx++;

	req = (struct reply *)tb->rx;
	printf("%s: cnt: %5d nr_rx: %5d\n", __func__, req->cnt, nr_rx);
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

	switch (opcode) {
	case OP_REQ_TEST:
		handle_test(tb);
		break;
	default:
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
static void dispatcher(void)
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

		/*
		 * Inline handling for now
		 * We will need to pass down the request once go SMP
		 */
		worker_handle_request(tw, tb);
	}
}

static void print_usage(void)
{
	printf("Usage ./board_emulator.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --dev=<name>                Specify the local network device\n"
	       "  --port=<port>               Specify the local UDP port we listen to\n"
	       "\n"
	       "Examples:\n"
	       "  ./board_emulator.o --port 8888 --dev=\"lo\" \n"
	       "  ./board_emulator.o -p 8888 -d ens4\n");
}

static struct option long_options[] = {
	{ "port",	required_argument,	NULL,	'p'},
	{ "dev",	required_argument,	NULL,	'd'},
	{ 0,		0,			0,	0  }
};

int main(int argc, char **argv)
{
	int ret;
	int c, option_index = 0;
	char ndev[32];
	bool ndev_set = false;
	int port = 0;

	/* Parse arguments */
	while (1) {
		c = getopt_long(argc, argv, "p:d:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			strncpy(ndev, optarg, sizeof(ndev));
			strncpy(global_net_dev, optarg, sizeof(global_net_dev));
			ndev_set = true;
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

	init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map);

	/* Same as host side init */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	/*
	 * add the special localhost board_info
	 */
	add_localhost_bi(&default_local_ei);

	ret = init_local_management_session(false);
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	dump_net_sessions();
	dispatcher();
}