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
#define NR_RUN_PER_THREAD 10000
static int test_size[] = { 1024 };

/*
 * We assign X threads to each board, meaning
 * they use will the vRegion belong to that board.
 */
#define NR_BOARDS		(2)
#define NR_THREADS_PER_BOARD	(10)

static int test_nr_threads[] = { NR_BOARDS*NR_THREADS_PER_BOARD };

/*
 * Each board has their own base addr,
 * which is used by threads belong to this group.
 */
unsigned long global_base_addr[NR_BOARDS];
struct board_info *global_bi[NR_BOARDS];

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
	struct session_net *mgmt_ses __maybe_unused;
	struct board_info *bi __maybe_unused;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	/*
	 * Get the pre allocated addr
	 */
	i = ti->id / NR_THREADS_PER_BOARD;
	addr = global_base_addr[i];
	bi = global_bi[i];

	legomem_getcpu(&cpu, &node);
	dprintf_CRIT("Thread id %d running on CPU %d. Base Addr %#lx, board: %s\n",
			ti->id, cpu, addr, bi->name);

	/*
	 * we will create a new session
	 * as the original one belongs to master thread
	 */
#if 1

	ses = find_or_alloc_vregion_session(ctx, addr);
	BUG_ON(!ses);
	send_buf = malloc(PAGE_SIZE);
	net_reg_send_buf(ses, send_buf, PAGE_SIZE);
#else
	ses = legomem_open_session_remote_mgmt(bi);
	send_buf = net_get_send_buf(ses);
#endif

	recv_buf = malloc(PAGE_SIZE);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		nr_tests = NR_RUN_PER_THREAD;

		pthread_barrier_wait(&thread_barrier);

#if 1
		legomem_write_sync(ctx, send_buf, addr, 0x10);

		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = __legomem_write_with_session(ctx, ses, send_buf, addr, size, LEGOMEM_WRITE_SYNC);
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
		pthread_barrier_wait(&thread_barrier);


#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			ret = legomem_read_with_session(ctx, ses,
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
#endif
	}
	return NULL;
}

int test_legomem_rw_multiboard(char *_unused)
{
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	for (i = 0; i < NR_BOARDS; i++) {
		struct session_net *ses;
		struct board_info *bi;
		size_t size;
		unsigned long __remote addr;

		/*
		 * The size must be larger than half of a vregion
		 * to make all vregions spread across different boards.
		 * Otherwise, api.c will just reuse an existing vregion.
		 *
		 * Also, prepopulate all pgtables.
		 */
		size = VREGION_SIZE / 2 + 1;

		addr = legomem_alloc(ctx, size, LEGOMEM_VM_FLAGS_POPULATE);
		if (addr < 0) {
			dprintf_ERROR("Fail to legomem alloc%d\n", 0);
			exit(9);
		}

		ses = find_or_alloc_vregion_session(ctx, addr);
		bi = ses->board_info;
		dprintf_CRIT("test board group %d is using: %s, addr range: [%#lx - %#lx]\n",
			i, bi->name, addr, addr + size);

		global_bi[i] = bi;
		global_base_addr[i] = addr;
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
