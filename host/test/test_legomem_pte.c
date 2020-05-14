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

#include <uapi/profile_point.h>
#include "../core.h"

int test_legomem_pte(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net *remote_mgmt_session;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;

	printf("%s(): test board %s\n", __func__, board_ip_port_str);

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

	struct legomem_common_headers resp;
	struct legomem_test_pte req;
	struct op_test_pte *op;
	struct lego_header *lego_header;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_TEST_PTE;

	op = &req.op;

	op->op = OP_REQ_TEST_PTE;
	op->pid = 1;
	op->start = 1<<22;
	op->end = 2<<22;

	net_send_and_receive(remote_mgmt_session, &req, sizeof(req),
			     &resp, sizeof(resp));

	return 0;
}

