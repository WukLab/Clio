/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/rbtree.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

#include "../core.h"

#define ALLOC_SIZE_4M	(1<<22)
#define ALLOC_SIZE_16M	(1<<24)
#define ALLOC_SIZE_64M	(1<<26)

void test_vm(void)
{
	unsigned long *addr;
	struct vregion_info *vi;
	struct proc_info *pi;
	unsigned int pid;
	int nr_allocs_per_vregion;
	int i, j;
	int alloc_size;
	struct timespec s, e;
	double diff_ns;

	pid = 100;

	pi = alloc_proc(pid, "proc_1", 0);
	if (!pi) {
		printf("fail to create the test pi\n");
		return;
	} 
	dump_procs();

	alloc_size = ALLOC_SIZE_4M;
	nr_allocs_per_vregion = VREGION_SIZE / alloc_size;

	addr = malloc(sizeof(*addr) * NR_VREGIONS * nr_allocs_per_vregion);

	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < NR_VREGIONS; i++) {
		vi = pi->vregion + i;
		for (j = 0; j < nr_allocs_per_vregion; j++) {
			int idx;

			idx = i * nr_allocs_per_vregion + j;
			addr[idx] = alloc_va_vregion(pi, vi, alloc_size, 0);		
			if (!addr[idx]) {
				dprintf_DEBUG("i %d j %d failed\n", i, j);
				exit(0);
			}
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	dprintf_INFO("alloc_size %d MB #nr_alloc=%d avg_ns=%lf\n",
		alloc_size >> 20, i * j, (diff_ns / i / j));

	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < NR_VREGIONS; i++) {
		vi = pi->vregion + i;
		for (j = 0; j < nr_allocs_per_vregion; j++) {
			int idx;

			idx = i * nr_allocs_per_vregion + j;
			free_va_vregion(pi, vi, addr[idx], alloc_size);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	dprintf_INFO("free_size %d MB #nr_free=%d avg_ns=%lf\n",
		alloc_size >> 20, i * j, (diff_ns / i / j));
}
