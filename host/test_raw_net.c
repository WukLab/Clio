/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing: raw network layer latency and throughput.
 * Using raw net layer means we skip any transport layer logic.
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

#define NR_RUN_PER_THREAD 100000

static struct board_info *remote_board;
static pthread_barrier_t thread_barrier;

#define NR_MAX_THREADS	(128)
/*
 * Tuning
 *
 * Sometimes I see packet lost issue when go multithreading
 * stats look like this:
 * Sender:
 *        TX: (X+N)
 *        RX: (X)
 *
 * Receiver:
 *        RX: (X)
 *
 * It means some packets from sender never reach receiver.
 */
//static int test_size[] = { 4, 16, 64, 256, 1024 };
//static int test_nr_threads[] = { 1, 2, 4, 8, 16};
static int test_size[] = { 64};
static int test_nr_threads[] = { 16 };
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

/*
 * BIG FAT NOTE: some steps for raw new testing
 *
 * 1. Have a host.o and monitor.o ready at two machines.
 * 2. Use host.o to send packet
 * 3. comment out the while (1) loop in dispatcher() [host.o]
 * 4. try to use net_receive instead of net_receive_zeropy in dispather() [monitor.o]
 * 5. Use scripts/test_raw_net.sh, modify ports, device etc
 */

static void *thread_func(void *_ti)
{
	struct legomem_pingpong_req *req;
	struct legomem_pingpong_req *resp;
	struct lego_header *lego_header;
	struct gbn_header *gbn_header;
	int i, j, nr_tests;
	struct timespec s, e;
	struct session_net *ses;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;
	double lat_ns;

	int max_buf_size = 1024*1024;

	ses = legomem_open_session_remote_mgmt(remote_board);
	if (!ses) {
		die("fail to open session. thread id %d\n", ti->id);
	}

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	getcpu(&cpu, &node);
	printf("%s(): thread id %d running on CPU %d, local session id %d remote session id %d\n",
		__func__,
		ti->id, cpu, get_local_session_id(ses), get_remote_session_id(ses));

	resp = malloc(max_buf_size);
	req = malloc(max_buf_size);
	net_reg_send_buf(ses, req, max_buf_size);

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_PINGPONG;

	gbn_header = to_gbn_header(req);
	gbn_header->type = GBN_PKT_DATA;
	set_gbn_src_dst_session(gbn_header, get_local_session_id(ses), 0);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		int send_size = test_size[i];

		req->reply_size = 0;

		/* need to include header size */
		send_size += sizeof(struct legomem_common_headers);

		/* Sync for every round */
		pthread_barrier_wait(&thread_barrier);

		nr_tests = NR_RUN_PER_THREAD;
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			raw_net_send(ses, req, send_size, NULL);
			raw_net_receive(resp, max_buf_size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		latency_ns[ti->id][i] = lat_ns;

#if 1
		dprintf_INFO("nr_tests: %d send_size: %u payload_size: %u avg: %lf ns\n",
			nr_tests, send_size, test_size[i], lat_ns / nr_tests);
#endif
	}
	legomem_close_session(NULL, ses);
	return NULL;
}

/*
 * Special note:
 *
 * To use this, we have to use transport bypass, otherwise
 * the packets will just be grabbed by GBN's background thread.
 *
 * However, this is not enough. Becuase host still has its mgmt background
 * thread. Once GBN is disabled, the `net_receive` within that thread
 * will be able receive anything. Thus, we need to diable that thread as well!
 */
int test_raw_net(char *board_ip_port_str)
{
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	if (transport_net_ops != &transport_bypass_ops) {
		dprintf_ERROR("Raw network testing needs bypass transport layer.\n"
		       "Please restart the test and pass \"--net_trans_ops=bypass\" %d\n", 0);
		return -1;
	}

	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	remote_board = find_board(ip, port);
	if (!remote_board) {
		dprintf_ERROR("Couldn't find the board_info for %s\n",
			board_ip_port_str);
		dump_boards();
		return -1;
	}
	printf("%s(): Using board %s\n", __func__, remote_board->name);

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
