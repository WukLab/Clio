/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing
 * - legomem_migration
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

int test_legomem_migration(void)
{
	struct legomem_context *ctx;
	struct timespec ts, te;
	struct board_info *dst_bi;
	unsigned long __remote addr;

	dprintf_INFO("Running migration test. %d\n", 0);

	ctx = legomem_open_context();
	dst_bi = monitor_bi;

#if 1
	addr = legomem_alloc(ctx, VREGION_SIZE-1, 0);
	if (!addr) {
		dprintf_ERROR("alloc failed %d\n", 0);
		legomem_close_context(ctx);
		return -1;
	}
#else
	addr = 0;
#endif

	clock_gettime(CLOCK_MONOTONIC, &ts);
	legomem_migration(ctx, dst_bi, addr, 128);
	clock_gettime(CLOCK_MONOTONIC, &te);

	return 0;
}
