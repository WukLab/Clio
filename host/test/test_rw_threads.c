/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

/*
 * Different from all other RW tests, in this one
 * the PID and Addr were already prepared by the board!
 * (see soc ctrl.c)
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
static double latency_read_ns[NR_MAX][NR_MAX];
static double latency_write_ns[NR_MAX][NR_MAX];
static double latency_read_tput[NR_MAX][NR_MAX];
static double latency_write_tput[NR_MAX][NR_MAX];

static int test_size[] = {1024};
static int test_nr_threads[] = { 1, 2, 4};
#define NR_RUN_PER_THREAD 256

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

struct legomem_context *ctx;
unsigned long global_base_addr;
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
	struct session_net *mgmt_ses __maybe_unused;
	struct board_info *bi __maybe_unused;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	legomem_getcpu(&cpu, &node);
	/* dprintf_CRIT("thread id %d running on CPU %d\n", ti->id, cpu); */

	/*
	 * XXX
	 * CHeck the soc output log
	 */
	//global_base_addr = 0x40000000;
	global_base_addr = 0x3e000000;
	addr = global_base_addr;

	// HACK! Tuneme during runtime.
	// Usually 3 is the board.
	bi = find_board_by_id(3);
	/* dprintf_INFO("Using board %s\n", bi->name); */

	ses = legomem_open_session_remote_mgmt(bi);
	send_buf = malloc(8192);
	net_reg_send_buf(ses, send_buf, 8192);

	recv_buf = malloc(8192);

	double latency;

	nr_tests = NR_RUN_PER_THREAD;

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		latency_write_ns[ti->id][i] = 0;

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = __legomem_write_with_session(ctx, ses, send_buf, addr, size, LEGOMEM_WRITE_SYNC);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_write_ns[ti->id][i] += 
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		latency = latency_write_ns[ti->id][i];
		latency_write_tput[ti->id][i] +=
			(NSEC_PER_SEC / (latency / j) * size * 8 / 1000000);
	}

#if 1
	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		latency_read_ns[ti->id][i] = 0;

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = legomem_read_with_session(ctx, ses, send_buf, recv_buf, addr, size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_read_ns[ti->id][i] += 
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		latency = latency_read_ns[ti->id][i];
		latency_read_tput[ti->id][i] +=
			(NSEC_PER_SEC / (latency / j) * size * 8 / 1000000);
	}
#endif
	return NULL;
}

/*
 * Flow:
 * 1) Create N threads
 * 2) Within each thread, repeatly run read or write for 1 million times, respectively
 * 3) Collect latency numbers
 * 4) Change number of concurrent threads, repeat 1-3 steps.
 */
int test_legomem_rw_threads(char *_unused)
{
	int k, i,j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	/*
	 * Prepare ctx and addr
	 * Check soc log.
	 */
	ctx = (struct legomem_context *)malloc(sizeof(*ctx));
	ctx->pid = 1;

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
			double sum = 0, avg_read, avg_write;
			double sum_tput_read = 0, sum_tput_write = 0;

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_read_ns[j][i];
				sum_tput_read += latency_read_tput[j][i];
			}
			avg_read = sum / nr_threads / NR_RUN_PER_THREAD;

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_write_ns[j][i];
				sum_tput_write += latency_write_tput[j][i];
			}
			avg_write = sum / nr_threads / NR_RUN_PER_THREAD;

			printf("#nr_theads=%3d Latency read=%10lf write=%10lf ns. Throughput read=%10lf write=%10lf Mbps\n",
				     nr_threads, avg_read, avg_write, sum_tput_read, sum_tput_write);
		}
	}

	printf("All tests are done.\n");
	exit(0);
	return 0;
}
