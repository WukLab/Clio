#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/opcode.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "core.h"

#define DEFAULT_TEST_SESSION 4

int test_legomem_soc(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net *ses_net;
	struct session_net *remote_mgmt_session;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;

	printf("%s(): test soc %s\n", __func__, board_ip_port_str);

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

	ses_net = net_open_session(&remote_board->local_ei,
				   &remote_board->remote_ei);

	ses_net->board_ip = remote_board->board_ip;
	ses_net->udp_port = remote_board->udp_port;
	ses_net->board_info = remote_board;
	ses_net->tid = gettid();

	set_local_session_id(ses_net, DEFAULT_TEST_SESSION);
	set_remote_session_id(ses_net, DEFAULT_TEST_SESSION);

	add_net_session(ses_net);
	board_add_session(remote_board, ses_net);

	struct msg {
		struct legomem_common_headers header;
		long cnt[50];
		long i;
	} __packed;
	struct msg *buf = (struct msg *)malloc(sizeof(struct msg) + 100);
	while (1) {
		net_receive(ses_net, buf, sizeof(struct msg) + 100);
		printf("%lu\n", buf->i);
	}

	return 0;
}
