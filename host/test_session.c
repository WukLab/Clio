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
#include <time.h>

#include "core.h"

#define NR_SESSIONS	(30)

int test_legomem_session(void)
{
	struct legomem_context *ctx = NULL;
	struct board_info *remote_board;
	struct session_net **ses_net;
	int i;
	int ret;

	printf("%s(): Running session test\n", __func__);
	/*
	 * Step I
	 * open a global unique context
	 */
	// ctx = legomem_open_context();
	// if (!ctx)
	// 	return -1;
	// dump_legomem_context();

	/*
	 * Step II
	 * Find a remote host we wanna talk with
	 */
	remote_board = find_board(ANY_BOARD, ANY_BOARD);
	if (!remote_board) {
		legomem_close_context(ctx);

		dprintf_ERROR("Please add more host/board before testing. "
			      "We cannot find any available boards. %d\n", 0);
		dump_boards();
		return -1;
	}
	printf("board %s\n", remote_board->name);

	/*
	 * Step III
	 * Start test
	 */
	ses_net = malloc(NR_SESSIONS * sizeof(*ses_net));
	if (!ses_net)
		return -1;
	memset(ses_net, 0, NR_SESSIONS * sizeof(*ses_net));

	struct timespec begin, end;

	for (i = 0; i < NR_SESSIONS; i++) {
		clock_gettime(CLOCK_MONOTONIC, &begin);
		ses_net[i] = legomem_open_session(ctx, remote_board);
		clock_gettime(CLOCK_MONOTONIC, &end);
		if (!ses_net[i]) {
			dprintf_ERROR("Fail to created the %dth session\n", i);
			ret = -1;
			goto cleanup;
		}
		dprintf_INFO("start %lu.%lu, end %lu.%lu\n", begin.tv_sec,
			     begin.tv_nsec, end.tv_sec, end.tv_nsec);
	}

	/*
	 * We are good
	 * Success
	 */
	ret = 0;

cleanup:
	for (i = 0; i < NR_SESSIONS; i++) {
		if (ses_net[i]) {
			clock_gettime(CLOCK_MONOTONIC, &begin);
			legomem_close_session(ctx, ses_net[i]);
			clock_gettime(CLOCK_MONOTONIC, &end);
			dprintf_INFO("start %lu.%lu, end %lu.%lu\n",
				     begin.tv_sec, begin.tv_nsec, end.tv_sec,
				     end.tv_nsec);
		}
	}
	legomem_close_context(ctx);
	return ret;
}
