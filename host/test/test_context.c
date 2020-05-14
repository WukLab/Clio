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

#include "../core.h"

#define NR_CONTEXT	128

int test_legomem_context(char *_unused)
{
	struct legomem_context **ctx;
	struct timespec ts, te;
	int i;

	printf("%s(): Running open/close test\n", __func__);

	ctx = malloc(sizeof(*ctx) * NR_CONTEXT);
	if (!ctx)
		return -ENOMEM;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < NR_CONTEXT; i++) {
		ctx[i] = legomem_open_context();
		if (!ctx[i]) {
			dprintf_ERROR("Fail to open contxt i=%d", i);
			return -EPERM;
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("open_context avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_CONTEXT, NR_CONTEXT);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < NR_CONTEXT; i++) {
		 legomem_close_context(ctx[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("close_context avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_CONTEXT, NR_CONTEXT);

	free(ctx);
	return 0;
}
