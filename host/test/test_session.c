/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing
 * - legomem_open_session
 * - legomem_close_session
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

#include "../core.h"

#define NR_SESSION	(128)

/*
 * If @board_ip_port_str is NULL, we wil
 */
int test_legomem_session(char *board_ip_port_str)
{
	struct legomem_context *ctx;
	struct board_info *remote_board;
	struct session_net **ses_net;
	int i;
	struct timespec ts, te;

	if (board_ip_port_str) {
		unsigned int ip, port;
		unsigned int ip1, ip2, ip3, ip4;

		sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
		ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

		remote_board = find_board(ip, port);
		if (!remote_board) {
			dprintf_ERROR("Couldn't find the board_info for %s\n",
				board_ip_port_str);
			dump_boards();
			return -1;
		}
	} else {
		remote_board = monitor_bi;
	}

	dprintf_INFO("Running session test. Remote Party: %s\n", remote_board->name);

	/*
	 * It's okay to have a NULL ctx
	 * just for testing purpose.
	 */
	ctx = NULL;

	ses_net = malloc(NR_SESSION * sizeof(*ses_net));
	if (!ses_net)
		return -1;
	memset(ses_net, 0, NR_SESSION * sizeof(*ses_net));

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < NR_SESSION; i++) {
		ses_net[i] = legomem_open_session(ctx, remote_board);
		if (!ses_net[i]) {
			dprintf_ERROR("Fail to created the %dth session\n", i);
			exit(1);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	dprintf_INFO("open_session avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_SESSION, NR_SESSION);


	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < NR_SESSION; i++) {
		legomem_close_session(ctx, ses_net[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	dprintf_INFO("close_session avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_SESSION, NR_SESSION);

	legomem_close_context(ctx);
	return 0;
}
