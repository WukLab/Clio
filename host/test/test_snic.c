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

#define NR_RUN_PER_THREAD 1000000

static struct board_info *remote_board;
static pthread_barrier_t thread_barrier;

#define NR_MAX_THREADS	(128)

#define TMP_NR_THREADS 8

/* Tuning */
/* static int test_size[] = { 4, 16, 64, 256, 512, 1024}; */
static int test_size[] = { 1024 };
static int test_nr_threads[] = { TMP_NR_THREADS };
static double latency_ns[128][128];
static double tput[128][128];

pthread_spinlock_t LOCK;
volatile int NR_TOTAL_REQS = 0;
volatile int NR_THRESHOLD = (NR_RUN_PER_THREAD * TMP_NR_THREADS);

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
	size_t recv_size;

	/*
	 * We will not contact remote for opening
	 * this mgmt session
	 */
	ses = legomem_open_session_remote_mgmt(remote_board);
	if (!ses) {
		die("fail to open session. thread id %d\n", ti->id);
	}

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	legomem_getcpu(&cpu, &node);
	printf("%s(): thread id %d running on CPU %d, local session id %d remote session id %d\n",
		__func__,
		ti->id, cpu, get_local_session_id(ses), get_remote_session_id(ses));

	req = malloc(max_buf_size);
	net_reg_send_buf(ses, req, max_buf_size);

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_PINGPONG;

	while (1) {
		for (i = 0; i < ARRAY_SIZE(test_size); i++) {
			int send_size = test_size[i];

			req->reply_size = 0;

			/* need to include header size */
			send_size += sizeof(struct legomem_common_headers);

			/* Do the work */
			nr_tests = NR_RUN_PER_THREAD;

			int k = 0;
			int nr_con = 1;
			clock_gettime(CLOCK_MONOTONIC, &s);
			for (j = 0; j < nr_tests; j++) {
				net_send(ses, req, send_size);
				/* net_receive_zerocopy(ses, (void **)&resp, &recv_size); */
			}
			clock_gettime(CLOCK_MONOTONIC, &e);

			lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
				 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

			latency_ns[ti->id][i] = lat_ns;

#if 1
			/* dprintf_INFO("thread id %d nr_tests: %d send_size: %u payload_size: %u avg: %f ns Throughput: %lf Gbps\n", */
			/*         ti->id, */
			/*         nr_tests, send_size, test_size[i], lat_ns / nr_tests, */
			/*         (NSEC_PER_SEC / (lat_ns / nr_tests) * send_size * 8 /1000000)/1000); */

			//gbps
			tput[ti->id][i] = (NSEC_PER_SEC / (lat_ns / nr_tests) * send_size * 8 /1000000)/1000;
#endif
		}

		pthread_spin_lock(&LOCK);
		NR_TOTAL_REQS += nr_tests;
		pthread_spin_unlock(&LOCK);

		/* Sync for every round */
		pthread_barrier_wait(&thread_barrier);
		/* double sum = 0; */
		/* for (i = 0; i < TMP_NR_THREADS; i++) { */
		/*         sum += tput[i][0]; */
		/* } */
		/* if (ti->id == 0) */
		/*         printf("%lf\n", sum); */

	}

	legomem_close_session(NULL, ses);

	while (1) ;
	return NULL;
}

/*
 * We run test against monitor rel stack.
 */
int test_snic(char *board_ip_port_str)
{
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	pthread_spin_init(&LOCK, PTHREAD_PROCESS_PRIVATE);

	if (board_ip_port_str) {
		unsigned int ip, port;
		unsigned int ip1, ip2, ip3, ip4;

		sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
		ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

		remote_board = find_board(ip, port);
		if (!remote_board) {
			dprintf_ERROR("Couldn't find the board_info for %s\n",
				board_ip_port_str);
			dump_boards();
			return -1;
		}
	} else {
		remote_board = monitor_bi;
	}
	dprintf_INFO("Remote Party: %s\n", remote_board->name);

	ti = malloc(sizeof(*ti) * NR_MAX_THREADS);
	tid = malloc(sizeof(*tid) * NR_MAX_THREADS);
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
			ti[i].cpu = mgmt_dispatcher_thread_cpu + i + 1;
			ti[i].id = i;
			ret = pthread_create(&tid[i], NULL, thread_func, &ti[i]);
			if (ret)
				die("fail to create test thread");
		}

		// HACK
		// Print tput at runtime
		int nr_cycle = 0;
		struct timespec s, e;
		double lat_ns;
		int nr_tests = NR_THRESHOLD;
		int nr_last_record = 0;
		while (1) {
			clock_gettime(CLOCK_MONOTONIC, &s);
			while (1) {
				volatile int tmp_NR = NR_TOTAL_REQS;
				volatile int RD = tmp_NR % NR_THRESHOLD;
				if ((RD==0)&&(tmp_NR>0) && (tmp_NR > nr_last_record)) {
					nr_last_record = tmp_NR;
					break;
				}
			}
			clock_gettime(CLOCK_MONOTONIC, &e);
			
			lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
				 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

			dprintf_INFO("cycle=%d nr_tests=%d AvgLatency: %f ns, IOPS = %lf (K) NR_TOTAL_THREADS=%d\n",
				nr_cycle, nr_tests, lat_ns / nr_tests,
				(NSEC_PER_SEC / (lat_ns / nr_tests)) / 1000,
				NR_TOTAL_REQS);

			nr_cycle ++;
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

			for (j = 0, sum = 0; j < nr_threads; j++) {
				sum += latency_ns[j][i];
			}
			avg = sum / nr_threads / NR_RUN_PER_THREAD;
			dprintf_INFO("#tests_per_thread=%10d #nr_theads=%3d #payload_size=%8d avg_RTT=%10lf ns\n",
					NR_RUN_PER_THREAD, nr_threads, send_size, avg);
		}
	}
	return 0;
}
