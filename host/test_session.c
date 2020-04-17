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

#include "core.h"

#define NR_SESSION	(128)

int test_legomem_session(void)
{
	struct legomem_context *ctx;
	struct board_info *remote_board;
	struct session_net **ses_net;
	int i;
	struct timespec ts, te;

	printf("%s(): Running session test\n", __func__);
	/*
	 * Step I
	 * open a global unique context
	 */
	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	/*
	 * Step II
	 * Use monitor is fine because all hosts are using the same stack.
	 */
	remote_board = monitor_bi;

	/*
	 * Step III
	 * Start test
	 */
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

	printf("open_session avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_SESSION, NR_SESSION);


	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < NR_SESSION; i++) {
		legomem_close_session(ctx, ses_net[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("close_session avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_SESSION, NR_SESSION);

	legomem_close_context(ctx);
	return 0;
}
