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
#include <stdarg.h>

#include "../core.h"

#define NR_RUN_PER_THREAD 1
#define NR_MAX 128
static int test_size[] = { 128, 256 };
static int test_nr_threads[] = { 1};
static double latency_alloc_ns[NR_MAX][NR_MAX];
static double latency_free_ns[NR_MAX][NR_MAX];

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

static void *thread_func(void *_ti)
{
	unsigned long __remote *addr;
	unsigned long size;
	int i, j, nr_tests;
	struct timespec s, e;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	addr = malloc(NR_RUN_PER_THREAD * sizeof(*addr));
	if (!addr)
		die("OOM");

	getcpu(&cpu, &node);
	printf("%s(): thread id %d running on CPU %d\n", __func__, ti->id, cpu);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];

		memset(addr, 0, NR_RUN_PER_THREAD * sizeof(*addr));
		nr_tests = NR_RUN_PER_THREAD;

		/* Sync for every round */
		pthread_barrier_wait(&thread_barrier);

		/* Run bunch alloc */
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			addr[j] = legomem_alloc(ctx, size, 0);
			if (unlikely(addr[j] == 0)) {
				dprintf_ERROR("thread id %d failed at %d\n",
					ti->id, j);
				break;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_alloc_ns[ti->id][i] =
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		/* Run bunch free */
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			if (addr[j])
				legomem_free(ctx, addr[j], size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_free_ns[ti->id][i] =
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

#if 1
		dprintf_INFO("thread id %d nr_tests: %d alloc_size: %lu avg_alloc: %lf ns avg_free: %lf ns\n",
			ti->id, nr_tests, size,
			latency_alloc_ns[ti->id][i] / nr_tests,
			latency_free_ns[ti->id][i] / nr_tests);
#endif
	}
	return NULL;
}

/*
 * Flow:
 * 1) Create N threads
 * 2) Within each thread, repeatly run alloc/free for 1 million times, respectively
 * 3) Collect latency numbers
 * 4) Change number of concurrent threads, repeat 1-3 steps.
 */
int test_legomem_alloc_free(void)
{
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	ti = malloc(sizeof(*ti) * NR_MAX);
	tid = malloc(sizeof(*tid) * NR_MAX);
	if (!tid || !ti)
		die("OOM");

	for (k = 0; k < ARRAY_SIZE(test_nr_threads); k++) {
		nr_threads = test_nr_threads[k];

		pthread_barrier_init(&thread_barrier, NULL, nr_threads);

		for (i = 0; i < nr_threads; i++) {
			/*
			 * cpu 0 is used for gbn polling now
			 * in case
			 */
			ti[i].cpu = i + 1;
			ti[i].id = i;
			ret = pthread_create(&tid[i], NULL, thread_func, &ti[i]);
			if (ret)
				die("fail to create test thread");
		}

		for (i = 0; i < nr_threads; i++) {
			pthread_join(tid[i], NULL);
		}

		/*
		 * Aggregate all stats
		 */
		for (i = 0; i < ARRAY_SIZE(test_size); i++) {
			int send_size = test_size[i];
			double sum, avg_alloc, avg_free;

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_alloc_ns[j][i];
			}
			avg_alloc = sum / nr_threads / NR_RUN_PER_THREAD;

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_free_ns[j][i];
			}
			avg_free = sum / nr_threads / NR_RUN_PER_THREAD;

			dprintf_INFO("#tests_per_thread=%10d #nr_theads=%3d #alloc_size=%8d "
				     "avg_alloc_RTT: %10lf ns avg_free_RTT: %10lf ns\n",
					NR_RUN_PER_THREAD, nr_threads, send_size, avg_alloc, avg_free);
		}
	}
	legomem_close_context(ctx);

	return 0;
}
