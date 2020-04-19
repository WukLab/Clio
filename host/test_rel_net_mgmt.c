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

#include "core.h"

#define NR_THREADS 2
#define NR_RUN_PER_THREAD 1000

struct board_info *remote_board;
static pthread_barrier_t thread_barrier;

/* This is the payload size */
#define NR_MAX_TEST_SIZE (16)
int test_size[] = { 4, 16, 64, 256, 1024 };
double latency_ns[NR_THREADS][NR_MAX_TEST_SIZE];

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

static void *thread_func(void *_ti)
{
	struct legomem_pingpong_req *req;
	struct legomem_pingpong_req *resp;
	struct lego_header *lego_header;
	double lat_ns;
	int i, j, nr_tests;
	struct timespec s, e;
	struct session_net *ses;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;

	int max_buf_size = 1024*1024;

	ses = legomem_open_session_remote_mgmt(remote_board);
	if (!ses) {
		die("fail to open session. thread id %d\n", ti->id);
	}

	getcpu(&cpu, &node);
	dprintf_INFO("thread id %d running on CPU %d, local session id %d remote session id %d\n",
		ti->id, cpu, get_local_session_id(ses), get_remote_session_id(ses));

	resp = malloc(max_buf_size);
	req = malloc(max_buf_size);
	net_reg_send_buf(ses, req, max_buf_size);

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_PINGPONG;

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		int send_size = test_size[i];

		req->reply_size = 0;

		/* need to include header size */
		send_size += sizeof(struct legomem_common_headers);

		/* Sync for every round */
		pthread_barrier_wait(&thread_barrier);

		/* Do the work */
		nr_tests = NR_RUN_PER_THREAD;
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			net_send_and_receive(ses, req, send_size, resp, max_buf_size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		latency_ns[ti->id][i] = lat_ns;

		dprintf_INFO("thread id %d nr_tests: %d send_size: %u payload_size: %u avg: %lf ns\n",
			ti->id,
			nr_tests, send_size, test_size[i], lat_ns / nr_tests);
	}
	return NULL;
}

/*
 * We run test against monitor rel stack.
 */
int test_rel_net_mgmt(void)
{
	int i, j, ret;
	int nr_threads = NR_THREADS;
	pthread_t tid[NR_THREADS];
	struct thread_info *ti;

	BUG_ON(ARRAY_SIZE(test_size) > NR_MAX_TEST_SIZE);

	if (transport_net_ops != &transport_gbn_ops) {
		dprintf_ERROR("Reliable network testing needs reliable transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=gbn\" %d\n", 0);
		return -1;
	}

	remote_board = monitor_bi;
	printf("%s(): Using board %s nr_threads=%d\n",
		__func__, remote_board->name, nr_threads);

	ti = malloc(sizeof(*ti) * NR_THREADS);
	if (!ti)
		die("OOM");

	pthread_barrier_init(&thread_barrier, NULL, NR_THREADS);

	for (i = 0; i < nr_threads; i++) {
		ti[i].id = i;
		ti[i].cpu = i;
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
		double sum, avg;

		for (j = 0, sum = 0; j < NR_THREADS; j++) {
			sum += latency_ns[j][i];
		}
		avg = sum / NR_THREADS / NR_RUN_PER_THREAD;
		dprintf_INFO("#nr_threads=%3d #tests_per_thread=%10d #payload_size=%8d avg_RTT=%10lf ns\n",
				NR_THREADS, NR_RUN_PER_THREAD, send_size, avg);
	}

	return 0;
}
