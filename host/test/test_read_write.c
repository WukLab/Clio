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

#define NR_MAX 128

#define OneM 1024*1024

/* Knobs */
#define NR_RUN_PER_THREAD 1000000
static int test_size[] = { 0x10 };
static int test_nr_threads[] = { 1, 2, 4 };

static double latency_read_ns[NR_MAX][NR_MAX];
static double latency_write_ns[NR_MAX][NR_MAX];

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
	int i, j, nr_tests;
	struct timespec s, e;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;
	int ret;
	struct session_net *ses;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	legomem_getcpu(&cpu, &node);
	printf("%s(): thread id %d running on CPU %d\n", __func__, ti->id, cpu);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		nr_tests = NR_RUN_PER_THREAD;

		addr = legomem_alloc(ctx, 16 * OneM, 0);
		dprintf_INFO("Remote region [%#lx - %#lx]\n", addr, addr + 16 * OneM);

		/* Prepare the send buf */
		ses = get_vregion_session(ctx, addr);
		BUG_ON(!ses);
		send_buf = malloc(VREGION_SIZE);
		recv_buf = malloc(VREGION_SIZE);
		net_reg_send_buf(ses, send_buf, VREGION_SIZE);

		sleep(5);

		/* Sync for every round */
		pthread_barrier_wait(&thread_barrier);

#if 1
		/* Run bunch sync write */
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = legomem_write_sync(ctx, send_buf, addr, size);
			if (unlikely(ret < 0)) {
				dprintf_ERROR(
					"thread id %d fail at %d, error code %d\n",
					ti->id, j, ret);
				break;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_write_ns[ti->id][i] =
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		dprintf_INFO("thread id %d nr_tests: %d write_size: %lu avg_write: %lf ns\n",
			ti->id, nr_tests, size,
			latency_write_ns[ti->id][i] / nr_tests);
#endif

#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = legomem_read(ctx, send_buf, recv_buf, addr, size);
			if (unlikely(ret < 0)) {
				dprintf_ERROR(
					"thread id %d fail at %d, error code %d\n",
					ti->id, j, ret);
				break;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		//if (addr)
		//	legomem_free(ctx, addr, size);

		latency_read_ns[ti->id][i] =
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		dprintf_INFO("thread id %d nr_tests: %d read_size: %lu avg_read: %lf ns\n",
			ti->id, nr_tests, size,
			latency_read_ns[ti->id][i] / nr_tests);
#endif

		free(send_buf);
		free(recv_buf);
	}
	return NULL;
}

/*
 * Flow:
 * 1) Create N threads
 * 2) Within each thread, repeatly run read or write for 1 million times, respectively
 * 3) Collect latency numbers
 * 4) Change number of concurrent threads, repeat 1-3 steps.
 */
int test_legomem_read_write(char *_unused)
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
			ret = pthread_create(&tid[i], NULL, thread_func_read, &ti[i]);
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
			double sum, avg_read, avg_write;

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_read_ns[j][i];
			}
			avg_read = sum / nr_threads / NR_RUN_PER_THREAD;

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_write_ns[j][i];
			}
			avg_write = sum / nr_threads / NR_RUN_PER_THREAD;

			dprintf_INFO("#tests_per_thread=%10d #nr_theads=%3d #alloc_size=%8d "
				     "avg_read_RTT: %10lf ns avg_write_RTT: %10lf ns\n",
					NR_RUN_PER_THREAD, nr_threads, send_size, avg_read, avg_write);
		}
	}
	legomem_close_context(ctx);

	return 0;
}
