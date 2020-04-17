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

/*
 * BIG FAT NOTE: some steps for raw new testing
 *
 * 1. Have a host.o and monitor.o ready at two machines.
 * 2. Use host.o to send packet
 * 3. comment out the while (1) loop in dispatcher() [host.o]
 * 4. try to use net_receive instead of net_receive_zeropy in dispather() [monitor.o]
 * 5. Use scripts/test_raw_net.sh, modify ports, device etc
 */

static void test_pingpong(struct board_info *bi, struct session_net *ses)
{
	struct legomem_pingpong_req *req;
	struct legomem_pingpong_req *resp;
	struct lego_header *lego_header;
	struct gbn_header *gbn_header;
	double lat_ns;
	int i, j, nr_tests;
	struct timespec s, e;

	int max_buf_size = 1024*1024;

	resp = malloc(max_buf_size);
	req = malloc(max_buf_size);
	net_reg_send_buf(ses, req, max_buf_size);

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_PINGPONG;

	gbn_header = to_gbn_header(req);
	gbn_header->type = GBN_PKT_DATA;
	set_gbn_src_dst_session(gbn_header, get_local_session_id(ses), 0);

	/* This is the payload size */
	int test_size[] = { 4, 16, 64, 256, 1024 };

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		int send_size = test_size[i];

		req->reply_size = 0;

		/* need to include header size */
		send_size += sizeof(struct legomem_common_headers);

		nr_tests = 1000;
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			//printf("i %d j %d\n", i, j);
			raw_net_send(ses, req, send_size, NULL);
			raw_net_receive(resp, max_buf_size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		printf("%s(): nr_tests: %d send_size: %u avg: %lf ns\n",
			__func__, nr_tests, send_size, lat_ns / nr_tests);
	}
}

/*
 * Special note:
 *
 * To use this, we have to use transport bypass, otherwise
 * the packets will just be grabbed by GBN's background thread.
 *
 * However, this is not enough. Becuase host still has its mgmt background
 * thread. Once GBN is disabled, the `net_receive` within that thread
 * will be able receive anything. Thus, we need to diable that thread as well!
 */
int test_raw_net(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net *remote_mgmt_session;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;

	if (transport_net_ops != &transport_bypass_ops) {
		dprintf_ERROR("Raw network testing needs to bypass transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=bypass\" %d\n", 0);
		return -1;
	}

	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	remote_board = find_board(ip, port);
	if (!remote_board) {
		dprintf_ERROR("Couldn't find the board_info for %s\n",
			board_ip_port_str);
		dump_boards();
		return -1;
	}
	printf("%s(): Using board %s\n", __func__, remote_board->name);

	/* Get our local endpoint for remote board's mgmt session */
	remote_mgmt_session = get_board_mgmt_session(remote_board);
	BUG_ON(!remote_mgmt_session);

	test_pingpong(remote_board, remote_mgmt_session);

	return 0;
}
