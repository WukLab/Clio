/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing: communication with board.
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

int test_legomem_board(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net **ses_net;
	struct session_net *remote_mgmt_session;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	int i;

	printf("%s(): test board %s\n", __func__, board_ip_port_str);

	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	/*
	 * Step II
	 * Find the testing board
	 */
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

	/*
	 * Step III.1
	 * Talk with remote board's mgmt session
	 */

#if 0
	 struct msg {
	 	struct legomem_common_headers comm_headers;
		int cnt;
	 } req, resp;

	 /*
	  * Send msg to remote mgmt session
	  */
	 net_send_and_receive(remote_mgmt_session, &req, sizeof(req),
			 &resp, sizeof(rsp));
#endif

	/*
	 * Step III.2
	 * Start test: open a lot sessions and then close
	 */
#define NR_SESSIONS 10
	ses_net = malloc(NR_SESSIONS * sizeof(*ses_net));
	if (!ses_net)
		return -1;
	memset(ses_net, 0, NR_SESSIONS * sizeof(*ses_net));

	for (i = 0; i < NR_SESSIONS; i++) {
		ses_net[i] = legomem_open_session(NULL, remote_board);
		if (!ses_net[i]) {
			dprintf_ERROR("Fail to created the %dth session\n", i);
			return -1;
		}
	}

	return 0;
}
