/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/page.h>
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

static int test_nr_threads[] = { 1};
static int test_size[] = { 16 };
//static int test_nr_threads[] = { 1 };

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
unsigned long global_base_addr;
static pthread_barrier_t thread_barrier;

static void *thread_func_read(void *_ti)
{
	unsigned long __remote addr;
	unsigned long size;
	int i, j, nr_tests;
	struct timespec s, e;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;
	int ret;
	struct session_net *ses;
	struct session_net *mgmt_ses __maybe_unused;
	struct board_info *bi __maybe_unused;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	legomem_getcpu(&cpu, &node);
	dprintf_CRIT("thread id %d running on CPU %d\n", ti->id, cpu);

#if 0
	addr = legomem_alloc(ctx, 4 * OneM, 0);
#else
	addr = global_base_addr;
#endif

	ses = find_or_alloc_vregion_session(ctx, addr);
	BUG_ON(!ses);

#define NR_CONNECTION (10)
	struct session_net *ses_array;
	void *send_buf, *recv_buf;

	ses_array = malloc(sizeof(*ses_array) * NR_CONNECTION);

	send_buf = malloc(4096);
	net_reg_send_buf(ses, send_buf, 4096);

	/* Create artificial connections */
	int base_sesid = get_local_session_id(ses);
	for (i = 0; i < NR_CONNECTION; i++) {
		ses_array[i] = *ses;
		ses_array[i]->session_id = base_sesid + i;

		printf("i %d sesid %d\n", i, ses_array[i]->session_id);
	}

	recv_buf = malloc(4096);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		nr_tests = NR_RUN_PER_THREAD;

#if 1
		for (j = 0; j < nr_tests; j++) {
			ses = &ses_array[j % NR_CONNECTION];

			clock_gettime(CLOCK_MONOTONIC, &s);
			ret = legomem_read_with_session(ctx, ses, send_buf, recv_buf, addr, size);
			clock_gettime(CLOCK_MONOTONIC, &e);

			latency_read_ns[ti->id][i] =
				(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
				(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		}

		dprintf_INFO("thread id %d nr_tests: %d read_size: %lu avg_read: %lf ns Throughput: %lf Mbps\n",
			ti->id, j, size,
			latency_read_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_read_ns[ti->id][i] / j) * size * 8 / 1000000));
#endif
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
int test_rw_same(char *_unused)
{
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	global_base_addr = legomem_alloc(ctx, 16 * OneM, LEGOMEM_VM_FLAGS_POPULATE);
	if (global_base_addr < 0) {
		dprintf_ERROR("Fail to legomem alloc%d\n", 0);
		exit(9);
	}

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
			ti[i].cpu = mgmt_dispatcher_thread_cpu + 1 + i;
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

	while (1);
	legomem_close_context(ctx);

	return 0;
}
