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

static void client_run(struct session_net *ses)
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

		nr_tests = 1;
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
 * It's similar to dispatcher(), since we are reusing
 * the PINGPONG opcode.
 */
static void server_run(struct session_net *ses)
{
	struct legomem_pingpong_req *req;
	struct legomem_pingpong_req *resp;
	struct lego_header *lego_header;
	int ret, opcode, reply_size;

	int max_buf_size = 1024*1024;

	resp = malloc(max_buf_size);
	req = malloc(max_buf_size);

	net_reg_send_buf(ses, resp, max_buf_size);

	while (1) {
		ret = net_receive(ses, req, max_buf_size);
		if (ret <= 0) {
			dprintf_ERROR("net error %d\n", ret);
			return;
		}

		lego_header = to_lego_header(req);
		opcode = lego_header->opcode;
		switch (opcode) {
		case OP_REQ_PINGPONG:
			reply_size = req->reply_size + sizeof(struct legomem_pingpong_resp);
			net_send(ses, resp, reply_size);
			break;
		default:
			if (1) {
				char err_msg[128];
				dump_packet_headers(req, err_msg);
				dprintf_ERROR("received unknown or un-implemented "
					      "opcode: %u (%s) packet dump: \n"
					      "%s\n",
					      opcode, legomem_opcode_str(opcode), err_msg);
			}
			return;
		}
	}
}

/*
 * We run test against monitor rel stack.
 * We use two [host] instances
 */
int test_rel_net_normal(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net *ses;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	bool client;

	if (transport_net_ops != &transport_gbn_ops) {
		dprintf_ERROR("Reliable network testing needs reliable transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=gbn\" %d\n", 0);
		return -1;
	}

	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	remote_board = find_board(ip, port);
	if (!remote_board) {
		dprintf_ERROR("Couldn't find the board_info for %s.\n"
			      "You can restart the test by passing \"--add_port=%s\"\n",
			board_ip_port_str, board_ip_port_str);
		dump_boards();
		return -1;
	}

	/*
	 * Client is the sender side of a session.
	 * Server is the receiver side of a session.
	 */
	client = true;

	if (client) {
		ses = legomem_open_session(NULL, remote_board);
		if (!ses) {
			dprintf_ERROR("Fail to open test net session with remote %s\n",
				board_ip_port_str);
			return -1;
		}
		dprintf_INFO("new session local_id: %d remote_id: %d\n",
			get_local_session_id(ses),
			get_remote_session_id(ses));

		client_run(ses);
	} else {
		/*
		 * In reality, the server does not know its local
		 * session id. The situation is similar to an RC case:
		 * the user application needs to use TCP/UDP to exchange
		 * the session ID in order to use our session.
		 *
		 * For simplicity, we assume our local session ID is 2.
		 */
		ses = find_net_session(2);
		if (!ses) {
			dprintf_ERROR("Local session is not allocated yet. %d\n", 0);
			return -1;
		}

		server_run(ses);
	}

	return 0;
}
