/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing
 * - legomem_migration
 *
 * Scripts
 * - scripts/test_migration.sh
 *
 * You need at least a monitor, a host running test case,
 * and either a real board or board emulator instance.
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

int test_legomem_migration(void)
{
	struct legomem_context *ctx;
	struct timespec ts, te;
	struct board_info *dst_bi;
	unsigned long __remote addr;

	dprintf_INFO("Running migration test. %d\n", 0);

	ctx = legomem_open_context();
	dst_bi = monitor_bi;

	addr = legomem_alloc(ctx, 128, 0);
	if (!addr) {
		dprintf_ERROR("alloc failed %d\n", 0);
		legomem_close_context(ctx);
		return -1;
	}

	clock_gettime(CLOCK_MONOTONIC, &ts);
	legomem_migration(ctx, dst_bi, addr, 128);
	clock_gettime(CLOCK_MONOTONIC, &te);

	return 0;
}
