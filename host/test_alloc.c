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


int test_legomem_alloc_free(void)
{
	struct legomem_context *ctx;
	struct timespec ts, te;
	unsigned long __remote addr;
	int nr_tests, i;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_context();

	nr_tests = 1;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < nr_tests; i++) {
		addr = legomem_alloc(ctx, 128, 0);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	legomem_close_context(ctx);
	return 0;
}
