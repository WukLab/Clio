/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing: raw network layer latency and throughput.
 * Using raw net layer means we skip any transport layer logic.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

#define MSEC_PER_SEC	1000L
#define USEC_PER_MSEC	1000L
#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L
#define USEC_PER_SEC	1000000L
#define NSEC_PER_SEC	1000000000L

static void test_pingpong(struct board_info *bi, struct session_net *ses)
{
	struct legomem_pingpong_req req;
	struct legomem_pingpong_req resp;
	struct lego_header *lego_header;
	struct gbn_header *gbn_header;
	double lat_ns;
	int i, nr_tests;
	struct timespec s, e;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_PINGPONG;

	gbn_header = to_gbn_header(&req);
	gbn_header->type = GBN_PKT_DATA;
	set_gbn_src_dst_session(gbn_header, get_local_session_id(ses), 0);

	nr_tests = 1000;
	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < nr_tests; i++) {
		raw_net_send(ses, &req, sizeof(req), NULL);
		raw_net_receive(&resp, sizeof(resp));
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	printf("%s(): nr_tests: %d avg: %lf ns\n",
		__func__, nr_tests, lat_ns / nr_tests);
}

int test_raw_net(void)
{
	struct board_info *remote_board;
	struct session_net *remote_mgmt_session;

	if (transport_net_ops != &transport_bypass_ops) {
		printf("%s(): Raw network testing needs to bypass transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=bypass\"\n",
		      	__func__);
		return -1;
	}

	/* Use monitor session */
	remote_board = monitor_bi;
	remote_mgmt_session = get_board_mgmt_session(remote_board);
	BUG_ON(!remote_mgmt_session);

	printf("%s(): Using board %s\n", __func__, remote_board->name);

	test_pingpong(remote_board, remote_mgmt_session);

	exit(1);
	return 0;
}
