/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
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

static void test_session_0_pingpong(struct board_info *bi, struct session_net *ses)
{
	struct legomem_pingpong_req *req;
	struct legomem_pingpong_req *resp;
	struct lego_header *lego_header;
	double lat_ns;
	int i, j, nr_tests;
	struct timespec s, e;

	int max_buf_size = 1024*1024;

	resp = malloc(max_buf_size);
	req = malloc(max_buf_size);
	net_reg_send_buf(ses, req, max_buf_size);

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_PINGPONG;

	/* This is the payload size */
	int test_size[] = { 1, 4, 16, 64, 256, 1024 };

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		int send_size = test_size[i];

		req->reply_size = 0;

		/* need to include header size */
		send_size += sizeof(struct legomem_common_headers);

		nr_tests = 10000;
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			net_send_and_receive(ses, req, send_size, resp, max_buf_size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		dprintf_INFO("nr_tests: %d send_size: %u payload_size: %u avg: %lf ns\n",
			nr_tests, send_size, test_size[i], lat_ns / nr_tests);
	}
}

/*
 * We run test against monitor rel stack.
 */
int test_rel_net_mgmt(void)
{
	struct board_info *remote_board;
	struct session_net *remote_mgmt_session;

	if (transport_net_ops != &transport_gbn_ops) {
		dprintf_ERROR("Reliable network testing needs reliable transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=gbn\" %d\n", 0);
		return -1;
	}

	remote_board = monitor_bi;
	remote_mgmt_session = get_board_mgmt_session(remote_board);
	BUG_ON(!remote_mgmt_session);

	printf("%s(): Using board %s\n", __func__, remote_board->name);

	test_session_0_pingpong(remote_board, remote_mgmt_session);

	return 0;
}
