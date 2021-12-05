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

static int test_nr_threads[] = { 1};

#define NR_MAX 128
static double latency_read_ns[NR_MAX][NR_MAX];
//static double latency_write_ns[NR_MAX][NR_MAX];

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

	global_base_addr = 0x3e000000;
	addr = global_base_addr;

#define NR_CONNECTION (1000)

	struct session_net **ses_array;
	void **send_buf, *recv_buf;

	// HACK! Tuneme during runtime.
	// Usually 3 is the board.
	bi = find_board_by_id(3);
	dprintf_INFO("Using board %s\n", bi->name);

	ses_array = malloc(sizeof(struct session_net *) * NR_CONNECTION);
	send_buf = malloc(4096);

	void *mr;
	for (i = 0; i < NR_CONNECTION; i++) {
		ses_array[i] = legomem_open_session_remote_mgmt(bi);
		if (ses_array[i] == NULL) {
			printf("Fail to create session on %d\n", i);
			exit(0);
		}
		if (i == 0) {
			net_reg_send_buf(ses_array[i], send_buf, 4096);
			struct session_raw_verbs *ses_verbs = (struct session_raw_verbs *)ses_array[i]->raw_net_private;
			mr = ses_verbs->send_mr;
		} else {
			struct session_raw_verbs *ses_verbs = (struct session_raw_verbs *)ses_array[i]->raw_net_private;
			ses_verbs->send_mr = mr;
			ses_verbs->send_buf = send_buf;
			ses_verbs->send_buf_size = 4096;
		}
	}

	recv_buf = malloc(4096);

	//static int session_array[] = { 1 };
	static int session_array[] = { 1, 8, 100, 400, 700, 1000};

/* Knobs */
#define NR_RUN_PER_THREAD 128

	printf("All sessions created, start read/write test..\n");
	for (i = 0; i < ARRAY_SIZE(session_array); i++) {
		//int NR_MAX_SESSION = session_array[i];
		int NR_MAX_SESSION = 1;
		size = 16;
		nr_tests = NR_RUN_PER_THREAD;

		latency_read_ns[ti->id][i] = 0;

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			void *buf = send_buf;
			ses = ses_array[j % NR_MAX_SESSION];
			//ret = legomem_read_with_session(ctx, ses, send_buf, recv_buf, addr, size);
			ret = __legomem_write_with_session(ctx, ses, buf, addr, size, LEGOMEM_WRITE_SYNC);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);
		latency_read_ns[ti->id][i] += 
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		dprintf_INFO("thread id %2d nr_tests: %2d size: %5lu nr_sessions: %4d avg_Write: %12lf ns Throughput: %lf Mbps\n",
			ti->id, j, size, session_array[i],
			latency_read_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_read_ns[ti->id][i] / j) * size * 8 / 1000000));
	}

#if 1
	for (i = 0; i < ARRAY_SIZE(session_array); i++) {
		//int NR_MAX_SESSION = session_array[i];
		int NR_MAX_SESSION = 1;
		size = 16;
		nr_tests = NR_RUN_PER_THREAD;

		latency_read_ns[ti->id][i] = 0;

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			void *buf = send_buf;
			ses = ses_array[j % NR_MAX_SESSION];
			ret = legomem_read_with_session(ctx, ses, buf, recv_buf, addr, size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);
		latency_read_ns[ti->id][i] += 
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		dprintf_INFO("thread id %2d nr_tests: %2d size: %5lu nr_sessions: %4d avg_Read: %12lf ns Throughput: %lf Mbps\n",
			ti->id, j, size, session_array[i],
			latency_read_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_read_ns[ti->id][i] / j) * size * 8 / 1000000));
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
int test_legomem_rw_processes(char *_unused)
{
	int k, i, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	//ctx = legomem_open_context();
	//if (!ctx)
	//	return -1;
	//dump_legomem_contexts();

	//global_base_addr = legomem_alloc(ctx, 4096, LEGOMEM_VM_FLAGS_POPULATE);
	//if (global_base_addr < 0) {
	//	dprintf_ERROR("Fail to legomem alloc%d\n", 0);
	//	exit(9);
	//}

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
	}

	printf("All tests are done.\n");
	exit(0);
	return 0;
}

