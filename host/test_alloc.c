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
	unsigned long __remote *addr;
	unsigned long size;
	int nr_tests, i;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	nr_tests = 3;
	size = 1024;
	addr = malloc(sizeof(*addr) * nr_tests);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < nr_tests; i++) {
		addr[i] = legomem_alloc(ctx, size, 0);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < nr_tests; i++) {
		legomem_free(ctx, addr[i], size);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	legomem_close_context(ctx);
	return 0;
}
