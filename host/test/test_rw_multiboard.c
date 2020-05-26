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
#include <sys/mman.h>
#include <infiniband/verbs.h>

#include "../core.h"
#include "../net/net.h"

#define NR_MAX 32

#define OneM 1024*1024

/* Knobs */
<<<<<<< HEAD
#define NR_RUN_PER_THREAD 1000000
static int test_size[] = { 1024 };
=======
#define NR_RUN_PER_THREAD 100000
static int test_size[] = { 1430 };
>>>>>>> host: add legomem_dist_barrier() api

/*
 * We assign X threads to each board, meaning
 * they use will the vRegion belong to that board.
 */
#define NR_BOARDS		(1)
#define NR_THREADS_PER_BOARD	(1)

static int test_nr_threads[] = { NR_BOARDS*NR_THREADS_PER_BOARD };

double read_tput[NR_BOARDS*NR_THREADS_PER_BOARD];
double write_tput[NR_BOARDS*NR_THREADS_PER_BOARD];

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

	struct msg_buf mb;
};

static struct legomem_context *ctx;
static pthread_barrier_t thread_barrier;

struct msg_buf mb_array[NR_MAX];

static void *thread_func_read(void *_ti)
{
	unsigned long __remote addr;
	unsigned long size;
	void *recv_buf;
	int i, j, nr_tests;
	struct timespec s, e;
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;
	struct session_net *ses;
	struct session_net *mgmt_ses __maybe_unused;
	struct board_info *bi __maybe_unused;
	struct msg_buf *mb;

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);

	int board_id = (ti->id / NR_THREADS_PER_BOARD) + 3;
	bi = find_board_by_id(board_id);

	addr = 0x3e000000;
	legomem_getcpu(&cpu, &node);
	dprintf_CRIT("Thread id %d running on CPU %d. Base Addr %#lx, board: %s\n",
			ti->id, cpu, addr, bi->name);

	/* Got per-thread msg buf */
	mb = &mb_array[ti->id];

	dprintf_INFO("t %d %#lx %#lx\n", ti->id, (u64)mb->buf, (u64)mb->private);

#if 1

	ses = find_or_alloc_vregion_session(ctx, addr);
	BUG_ON(!ses);
#else
	ses = legomem_open_session_remote_mgmt(bi);
#endif

	recv_buf = malloc(4096);

#if 0
	sleep(5);
	dprintf_CRIT("Enter barrier wait..%d\n", 0);
	legomem_dist_barrier();
	dprintf_CRIT("Exit barrier.. %d\n", 0);
#endif

	for (i = 0; i < ARRAY_SIZE(test_size); i++) {
		size = test_size[i];
		nr_tests = NR_RUN_PER_THREAD;
	
#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			__legomem_write_with_session_msgbuf(ctx, ses, mb, addr, size, LEGOMEM_WRITE_SYNC);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_write_ns[ti->id][i] = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
					      (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		dprintf_INFO("thread id %d nr_tests: %d write_size: %lu avg_write: %lf ns Throughput: %lf Mbps\n",
			ti->id, j, size,
			latency_write_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_write_ns[ti->id][i] / j) * size * 8 / 1000000));

		write_tput[ti->id] = (NSEC_PER_SEC / (latency_write_ns[ti->id][i] / j) * size * 8 / 1000000);
#endif


		pthread_barrier_wait(&thread_barrier);
#if 1
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < nr_tests; j++) {
			legomem_read_with_session_msgbuf(ctx, ses, mb, recv_buf, addr, size);
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency_read_ns[ti->id][i] = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
					      (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		dprintf_INFO("thread id %d nr_tests: %d read_size: %lu avg_read: %lf ns Throughput: %lf Mbps\n",
			ti->id, j, size,
			latency_read_ns[ti->id][i] / j,
			(NSEC_PER_SEC / (latency_read_ns[ti->id][i] / j) * size * 8 / 1000000));

		read_tput[ti->id] = (NSEC_PER_SEC / (latency_read_ns[ti->id][i] / j) * size * 8 / 1000000);
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
	void *recv_buf;
	struct ibv_mr *mr;

	BUILD_BUG_ON(NR_BOARDS * NR_THREADS_PER_BOARD > NR_MAX);

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

#if 0
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
		//size = VREGION_SIZE / 2 + 1;
		size = PAGE_SIZE;

		addr = legomem_alloc(ctx, size, LEGOMEM_VM_FLAGS_POPULATE);
		if (addr < 0) {
			dprintf_ERROR("Fail to legomem alloc%d\n", 0);
			exit(9);
		}

		ses = find_or_alloc_vregion_session(ctx, addr);
		bi = ses->board_info;
		dprintf_ERROR("Test board group %d is Using: %s, addr range: [%#lx - %#lx]\n",
			i, bi->name, addr, addr + size);

		global_bi[i] = bi;
		global_base_addr[i] = addr;
	}
#endif

	ti = malloc(sizeof(*ti) * NR_MAX);
	tid = malloc(sizeof(*tid) * NR_MAX);
	if (!tid || !ti)
		die("OOM");

	/*
	 * Prepare msgbuf
	 * Assume 4K per thread
	 */
	int max_buf_size = 4096 * NR_MAX;
	recv_buf = mmap(0, max_buf_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB,
			0, 0);
	if (recv_buf == MAP_FAILED) {
		perror("mmap");
		dprintf_ERROR("Fail to allocate memory %d\n", errno);
		exit(0);
	}

	mr = raw_verbs_reg_mr(recv_buf, max_buf_size);
	if (!mr)
		exit(0);

	for (i = 0; i < NR_MAX; i++) {
		struct msg_buf *mb = &mb_array[i];

		mb->buf = recv_buf + i * 4096;
		mb->max_buf_size = 4096;
		mb->private = (void *)mr;

		dprintf_INFO("index %d buf %#lx private %#lx\n",
			i, (u64)mb->buf, (u64)mb->private);
	}

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

	double rs=0, ws=0;
	for (i = 0; i < NR_BOARDS * NR_THREADS_PER_BOARD ; i++) {
		rs += read_tput[i];
		ws += write_tput[i];
	}
	printf("read %f Mbps write %f Mbps\n", rs, ws);

	while (1);
	legomem_close_context(ctx);

	return 0;
}
