/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/page.h>
#include <uapi/tlb.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "../core.h"

#define NR_MAX 128

static int test_nr_threads[] = { 1 };
static double latency_write_ns[NR_MAX][NR_MAX];

static int NR_PAGES, NR_ROUNDS;

static inline void die(const char * str, ...)
{
	va_list args;
	va_start(args, str);
	vfprintf(stderr, str, args);
	fputc('\n', stderr);
	exit(1);
}

struct thread_info {
	int id;
	int cpu;
};

static struct legomem_context *ctx;
static pthread_barrier_t thread_barrier;

static void *thread_func_read(void *_ti)
{
	unsigned long __remote addr;
	unsigned long size;
	void *send_buf, *recv_buf;
	int i, j;
	struct timespec s, e;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;
	int ret;
	struct session_net *ses;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);
	legomem_getcpu(&cpu, &node);
	dprintf_CRIT("Runs on CPU %d\n", cpu);

	size = 4;

	NR_ROUNDS = 1000000;
	for (i = 0; i < NR_ROUNDS; i++) {
		/*
		 * Allocate a bunch pages that are not pre-populated.
		 * flag must be 0.
		 */
		addr = legomem_alloc(ctx, NR_PAGES * PAGE_SIZE, 0);

		ses = find_or_alloc_vregion_session(ctx, addr);
		BUG_ON(!ses);

		send_buf = malloc(VREGION_SIZE);
		recv_buf = malloc(VREGION_SIZE);
		net_reg_send_buf(ses, send_buf, VREGION_SIZE);

#if 0
		dprintf_INFO("thread id %d, ses_id %d region [%#lx - %#lx] ROUND %d \n",
				ti->id, get_local_session_id(ses),
				addr, addr + NR_PAGES * PAGE_SIZE, i);
#endif

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < NR_PAGES; j++) {
			unsigned long _addr;
			_addr = j * PAGE_SIZE + addr;
			dprintf_INFO(" %d \n", j);

			ret = __legomem_write_with_session(ctx, ses, send_buf, _addr, size,
							  LEGOMEM_WRITE_SYNC);
			if (unlikely(ret < 0)) {
				dprintf_ERROR(
					"thread id %d fail at %d, error code %d\n",
					ti->id, j, ret);
				exit(0);
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		/*
		 * Now free those pages,
		 * remote will flush tlb too.
		 */
		legomem_free(ctx, addr, NR_PAGES * PAGE_SIZE);

		latency_write_ns[ti->id][0] +=
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
	}

	dprintf_INFO("NR_ROUNDS %d NR_PAGES/round %d size: %lu avg: %lf ns\n",
		NR_ROUNDS, NR_PAGES, size,
		latency_write_ns[ti->id][0] / (NR_ROUNDS * NR_PAGES));

	return NULL;
}

int test_legomem_rw_fault(char *_unused)
{
	int k, i, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	//NR_PAGES = LEGOMEM_NR_TLB_ENTRIES + 100;
	NR_PAGES = 40;
	printf("nr_pages %d  total size %#lx\n", NR_PAGES, NR_PAGES * PAGE_SIZE);

	ti = malloc(sizeof(*ti) * NR_MAX);
	tid = malloc(sizeof(*tid) * NR_MAX);
	if (!tid || !ti)
		die("OOM");
	for (k = 0; k < ARRAY_SIZE(test_nr_threads); k++) {
		nr_threads = test_nr_threads[k];
		pthread_barrier_init(&thread_barrier, NULL, nr_threads);
		for (i = 0; i < nr_threads; i++) {
			ti[i].cpu = mgmt_dispatcher_thread_cpu + i + 1;
			ti[i].id = i;
			ret = pthread_create(&tid[i], NULL, thread_func_read, &ti[i]);
			if (ret)
				die("fail to create test thread");
		}
		for (i = 0; i < nr_threads; i++) {
			pthread_join(tid[i], NULL);
		}
	}

	legomem_close_context(ctx);

	return 0;
}
