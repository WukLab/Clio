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

#define NR_RUN_PER_THREAD 100

static struct board_info *remote_board;
static pthread_barrier_t thread_barrier;

#define NR_MAX_THREADS	(128)
/* Tuning */
static int test_size[] = { 1400 };
static int test_nr_threads[] = { 1};
static double latency_ns[128][128];

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


//#define INLINE_RECEIVE

#ifdef INLINE_RECEIVE
	/*
	 * In order to run inline handling
	 * we should stop polling threads
	 */
	sleep(1);
	WRITE_ONCE(stop_gbn_poll_thread, true);
	WRITE_ONCE(stop_mgmt_dispatcher_thread, true);
	sleep(2);
	dprintf_INFO("Both threads should have stopped now.. %d\n", 0);
#endif

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		int send_size = test_size[i];

		req->reply_size = 0;

		/* need to include header size */
		send_size += sizeof(struct legomem_common_headers);

		/* Sync for every round */
		pthread_barrier_wait(&thread_barrier);

		/* Do the work */
		nr_tests = NR_RUN_PER_THREAD;

#ifdef INLINE_RECEIVE

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			int ret;

			net_send(ses, req, send_size);

retry:
			ret = raw_net_receive_zerocopy((void **)&resp, &recv_size);
			if (ret == 0)
				goto retry;
			if (0) {
				char packet_dump_str[256];
				dump_packet_headers(resp, packet_dump_str);
				printf("%s\n", packet_dump_str);
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &e);
#else

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			net_send_and_receive_zerocopy(ses, req, send_size, (void **)&resp, &recv_size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

#endif

		lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		latency_ns[ti->id][i] = lat_ns;

#if 1
		dprintf_INFO("thread id %d nr_tests: %d send_size: %u payload_size: %u avg: %f ns Throughput: %lf Mbps\n",
			ti->id,
			nr_tests, send_size, test_size[i], lat_ns / nr_tests,
			(NSEC_PER_SEC / (lat_ns / nr_tests) * send_size * 8 /1000000));
#endif
	}
	legomem_close_session(NULL, ses);

	while (1) ;
	return NULL;
}

/*
 * We run test against monitor rel stack.
 */
int test_rel_net_mgmt(char *board_ip_port_str)
{
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	if (transport_net_ops != &transport_gbn_ops) {
		dprintf_ERROR("Reliable network testing needs reliable transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=gbn\" %d\n", 0);
		return -1;
	}

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
