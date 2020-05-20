/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static int test_size[] = { 4, 16, 64, 256, 512, 1024, 2048, 4096 };

int main(void)
{
	int i, j, nr_run;
	char *src, *dst;
	struct timespec s, e;
	double lat;

	src = malloc(10000);
	dst = malloc(10000);
	memset(src, 0, 10000);
	memset(dst, 0, 10000);

	nr_run = 100000000;

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_run; j++) {
			memcpy(dst, src, test_size[i]);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		
		lat = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) - (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		printf("test_size %d nr_run %d avg %lf ns\n",
			test_size[i], nr_run, lat / nr_run);
	}
}
