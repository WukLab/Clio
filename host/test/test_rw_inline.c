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

static int test_size[] = { 4, 16, 64, 256, 512, 1024, 2048, 4096 };
static int test_nr_threads[] = { 1 };

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

/*
 * Basically a copy of legomem_read.
 * replaced net receive part
 */
static int inline_legomem_read_with_session(struct legomem_context *ctx, struct session_net *ses,
			      void *send_buf, void *recv_buf,
			      unsigned long __remote addr, size_t total_size)
{
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp *resp;
	struct lego_header *tx_lego;
	struct lego_header *rx_lego;
	int nr_sent, ret, i;
	size_t sz, recv_size;

	req = send_buf;
	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_READ;
	tx_lego->size = sizeof(*req) - LEGO_HEADER_OFFSET;

	nr_sent = 0;
	do {
		if (total_size >= max_lego_payload)
			sz = max_lego_payload;
		else
			sz = total_size;
		total_size -= sz;

		tx_lego->tag = nr_sent;
		req->op.va = addr + nr_sent * max_lego_payload;
		req->op.size = sz;

		ret = net_send(ses, req, sizeof(*req));
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to send read at nr_sent: %d\n", nr_sent);
			break;
		}
		nr_sent++;
	} while (total_size);

	/* Shift to start of payload */
	recv_buf += sizeof(*resp);
	for (i = 0; i < nr_sent; i++) {
		//ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);

retry:
		ret = raw_net_receive_zerocopy((void **)&resp, &recv_size);
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to recv read at %dth reply\n", i);
			break;
		} else if (ret == 0)
			goto retry;

		/* Sanity Checks */
		rx_lego = to_lego_header(resp);
		if (unlikely(rx_lego->req_status != 0)) {
			dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
			continue;
		}
		if (unlikely(rx_lego->opcode != OP_REQ_READ_RESP)) {
			dprintf_ERROR("errnor: invalid resp msg %s\n",
				legomem_opcode_str(rx_lego->opcode));
			continue;
		}

#if 0
		/* Minus header to get lego payload size */
		recv_size -= sizeof(*resp);
		memcpy(recv_buf, resp->ret.data, recv_size);
		recv_buf += recv_size;
#endif
	}
	return 0;
}

/*
 * Copy of legomem read
 * replaced net receive part
 */
int inline_legomem_write_with_session(struct legomem_context *ctx, struct session_net *ses,
				 void *send_buf, unsigned long __remote addr, size_t total_size,
				 enum legomem_write_flag flag)
{
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp *resp;
	size_t recv_size, sz; 
	struct lego_header *tx_lego;
	struct lego_header *rx_lego;
	int i, ret, nr_sent;

	nr_sent = 0;
	do {
		u64 shift;

		if (total_size >= max_lego_payload)
			sz = max_lego_payload;
		else
			sz = total_size;
		total_size -= sz;

		/*
		 * Shift to next pkt start
		 * We will override portion of already-sent user data
		 */
		shift = (u64)(nr_sent * max_lego_payload);
		req = (struct legomem_read_write_req *)((u64)send_buf + shift);
		req->op.va = addr + shift;
		req->op.size = sz;
		tx_lego = to_lego_header(req);
		tx_lego->pid = ctx->pid;
		if (flag == LEGOMEM_WRITE_SYNC)
			tx_lego->opcode = OP_REQ_WRITE;
		else if (flag == LEGOMEM_WRITE_ASYNC)
			tx_lego->opcode = OP_REQ_WRITE_NOREPLY;

		ret = net_send(ses, req, sz + sizeof(*req));
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to send write at nr_sent: %d\n", nr_sent);
			break;
		}
		nr_sent++;
	} while (total_size);

	if (flag == LEGOMEM_WRITE_ASYNC)
		return 0;

	for (i = 0; i < nr_sent; i++) {
		//ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);

retry:
		ret = raw_net_receive_zerocopy((void **)&resp, &recv_size);
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to recv write at %dth reply\n", i);
			break;
		} else if (ret == 0)
			goto retry;

		/* Sanity Checks */
		rx_lego = to_lego_header(resp);
		if (unlikely(rx_lego->req_status != 0)) {
			dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
			continue;
		}
		if (unlikely(rx_lego->opcode != OP_REQ_WRITE_RESP)) {
			dprintf_ERROR("errnor: invalid resp msg %s. at %dth reply\n",
				legomem_opcode_str(rx_lego->opcode), i);
			continue;
		}
	}
	return 0;
}


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
	dprintf_CRIT("thread id %d running on CPU %d\n", ti->id, cpu);

	addr = legomem_alloc(ctx, 4 * OneM, LEGOMEM_VM_FLAGS_POPULATE);

	ses = find_or_alloc_vregion_session(ctx, addr);
	BUG_ON(!ses);

	/*
	 * In fact, we can only use mgmt session
	 * coz we will stop gbn poll and we will poll data
	 * no one is handling acks.
	 */
#if 0
	send_buf = malloc(VREGION_SIZE);
	net_reg_send_buf(ses, send_buf, VREGION_SIZE);
#else
	bi = ses->board_info;
	ses = legomem_open_session_remote_mgmt(bi);
	send_buf = net_get_send_buf(ses);
#endif
	recv_buf = malloc(VREGION_SIZE);

	/*
	 * In order to run inline handling
	 * we should stop polling threads
	 */
	sleep(1);
	WRITE_ONCE(stop_gbn_poll_thread, true);
	WRITE_ONCE(stop_mgmt_dispatcher_thread, true);
	sleep(2);
	dprintf_INFO("Both threads should have stopped now.. %d\n", 0);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		nr_tests = NR_RUN_PER_THREAD;

		/*
		 * READ
		 */
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = inline_legomem_read_with_session(ctx, ses,
							send_buf, recv_buf, addr, size);
			if (unlikely(ret < 0)) {
				dprintf_ERROR(
					"thread id %d fail at %d, error code %d\n",
					ti->id, j, ret);
				break;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_read_ns[ti->id][i] =
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		dprintf_INFO("thread id %d nr_tests: %d read_size: %lu avg_read: %lf ns Throughput: %lf Mbps\n",
			ti->id, j, size,
			latency_read_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_read_ns[ti->id][i] / j) * size * 8 / 1000000));

		/*
		 * WRITE
		 */
#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = inline_legomem_write_with_session(ctx, ses,
								send_buf, addr, size,
								LEGOMEM_WRITE_SYNC);
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
		dprintf_INFO("thread id %d nr_tests: %d write_size: %lu avg_write: %lf ns Throughput: %lf Mbps\n",
			ti->id, j, size,
			latency_write_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_write_ns[ti->id][i] / j) * size * 8 / 1000000));
#endif
	}
	return NULL;
}

int test_legomem_rw_inline(char *_unused)
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
			ti[i].cpu = mgmt_dispatcher_thread_cpu + 1;
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

	return 0;
}
