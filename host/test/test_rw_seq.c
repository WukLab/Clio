/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/page.h>
#include <uapi/tlb.h>
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
//static int test_size[] = { 256 };
//static int test_nr_threads[] = { 8 };

static int test_size[] = { 4, 16, 64, 256, 512, 1024, 2048, 4096 };
static int test_nr_threads[] = { 1 };

static double latency_read_ns[NR_MAX][NR_MAX];
static double latency_write_ns[NR_MAX][NR_MAX];

static int NR_PAGES;

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

	addr = global_base_addr;
	ses = find_or_alloc_vregion_session(ctx, addr);
	BUG_ON(!ses);

	send_buf = malloc(VREGION_SIZE);
	recv_buf = malloc(VREGION_SIZE);
	net_reg_send_buf(ses, send_buf, VREGION_SIZE);

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		nr_tests = NR_RUN_PER_THREAD;

		dprintf_INFO("thread id %d, ses_id %d region [%#lx - %#lx]\n",
				ti->id, get_local_session_id(ses),
				addr, addr + 16 * OneM);

#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			unsigned long _addr;
			_addr = (j % NR_PAGES) * PAGE_SIZE + addr;
			//printf("%d %#lx\n", j, _addr);
			ret = __legomem_write_with_session(ctx, ses, send_buf, _addr, size, LEGOMEM_WRITE_SYNC);
			if (unlikely(ret < 0)) {
				dprintf_ERROR(
					"thread id %d fail at %d, error code %d\n",
					ti->id, j, ret);
				exit(0);
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

#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			unsigned long _addr;
			_addr = (j % NR_PAGES) * PAGE_SIZE + addr;
			ret = legomem_read_with_session(ctx, ses,
							send_buf, recv_buf, _addr, size);
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

/*
 * Flow:
 * 1) Create N threads
 * 2) Within each thread, repeatly run read or write for 1 million times, respectively
 * 3) Collect latency numbers
 * 4) Change number of concurrent threads, repeat 1-3 steps.
 */
int test_legomem_rw_seq(char *_unused)
{
	int k, i, j, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	/*
	 * Make a working set larger than tlb.
	 */
	NR_PAGES = LEGOMEM_NR_TLB_ENTRIES + 100;

	global_base_addr = legomem_alloc(ctx, NR_PAGES * PAGE_SIZE, LEGOMEM_VM_FLAGS_POPULATE);
	//global_base_addr = legomem_alloc(ctx, NR_PAGES * PAGE_SIZE, 0);

	printf("nr_pages %d  total size %#lx\n", NR_PAGES, NR_PAGES * PAGE_SIZE);

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

	legomem_free(ctx, global_base_addr, NR_PAGES * PAGE_SIZE);
	legomem_close_context(ctx);

	return 0;
}
