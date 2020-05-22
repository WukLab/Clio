/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing
 * - legomem_multiversion_object
 *
 * Scripts
 * - scripts/verobj_pattern_generator.py
 *
 * Need to run the scripts to generate a request pattern file
 * and feed it into this test as argument
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include "../core.h"
#include "../api.h"

#define NR_MAX		128
const int nr_threads = 32;
const char req_filename[] = "../scripts/req_pattern.log";

/* request related */
static const int data_size = 1024;
static int nr_reqs = 0;
struct req_info {
	int objid;
	int rw;
};
static struct req_info *req_sequence;
static unsigned int *objid_array;

/* stats related */
struct req_stats {
	int nr_create;
	int nr_read;
	int nr_write;
	int nr_delete;
};
static struct req_stats all_req_stats[NR_MAX];
static double latency_create_ns[NR_MAX];
static double latency_read_ns[NR_MAX];
static double latency_write_ns[NR_MAX];
static double latency_delete_ns[NR_MAX];
static char* perthread_buffer[NR_MAX];

/* threading related */
struct thread_info {
	int id;
	int cpu;
	int objid_array_start_idx;
	int objid_array_end_idx;	// inclusive
	int seq_start_idx;
	int seq_end_idx;		// inclusive
};
static struct legomem_context *ctx;
static pthread_barrier_t thread_barrier;

static void *thread_func(void *_info)
{
	struct thread_info *info = (struct thread_info *)_info;
	struct timespec s, e;
	int cpu, node, i, ret, objid;

	if (pin_cpu(info->cpu)) {
		printf("can not pin to cpu %d\n", info->cpu);
		exit(-1);
	}

	legomem_getcpu(&cpu, &node);
	printf("%s(): thread id %d running on CPU %d\n", __func__, info->id, cpu);

	for (i = info->objid_array_start_idx; i <= info->objid_array_end_idx; i++) {
		clock_gettime(CLOCK_MONOTONIC, &s);
		ret = legomem_mv_create(ctx, data_size, 0, &objid_array[i]);
		if (ret < 0) {
			printf("thread %d failed at CREATE request %d\n", info->id, i);
			break;
		}
		clock_gettime(CLOCK_MONOTONIC, &e);
		latency_create_ns[info->id] +=
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		all_req_stats[info->id].nr_create++;
	}

	/* wait all create request to finish */
	pthread_barrier_wait(&thread_barrier);

	/* read/write request */
	for (i = info->seq_start_idx; i <= info->seq_end_idx; i++) {
		objid = objid_array[req_sequence[i].objid];
		clock_gettime(CLOCK_MONOTONIC, &s);
		if (req_sequence[i].rw == 0) {
			ret = legomem_mv_read_latest(ctx, objid, data_size,
				perthread_buffer[info->id]);
			all_req_stats[info->id].nr_read++;
		} else {
			ret = legomem_mv_write(ctx, objid, data_size,
				perthread_buffer[info->id]);
			all_req_stats[info->id].nr_write++;
		}
		if (ret < 0) {
			printf("thread %d failed at READ/WRITE request %d\n", info->id, i);
			break;
		}

		clock_gettime(CLOCK_MONOTONIC, &e);
		if (req_sequence[i].rw == 0) {
			latency_read_ns[info->id] +=
				(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
				(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		} else {
			latency_write_ns[info->id] +=
				(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
				(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		}
	}

	/* wait all read/write finish request to finish */
	pthread_barrier_wait(&thread_barrier);

	for (i = info->objid_array_start_idx; i <= info->objid_array_end_idx; i++) {
		clock_gettime(CLOCK_MONOTONIC, &s);
		ret = legomem_mv_delete(ctx, objid_array[i]);
		if (ret < 0) {
			printf("thread %d failed at DELETE request %d\n", info->id, i);
			break;
		}
		clock_gettime(CLOCK_MONOTONIC, &e);
		latency_delete_ns[info->id] +=
			(e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			(s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		all_req_stats[info->id].nr_delete++;
	}

	return NULL;
}

int test_legomem_multiver_obj(char *_unused)
{
	int objid, rw, objid_max = INT_MIN;
	int ret = 0, i;
	size_t n;
	FILE *fptr;
	pthread_t *tid;
	struct thread_info *ti;
	double sum_create = 0, sum_delete = 0, sum_read = 0, sum_write = 0;
	double avg_create = 0, avg_delete = 0, avg_read = 0, avg_write = 0;
	int nr_create = 0, nr_delete = 0, nr_read = 0, nr_write = 0;

	fptr = fopen(req_filename, "r");
	if (!fptr) {
		printf("File: %s doesn't exist\n", req_filename);
		return -1;
	}

	/* parse request log */
	do {
		n = fscanf(fptr, "%d\t%d\n", &objid, &rw);
		if (n == 2) {
			nr_reqs++;
			if (objid > objid_max)
				objid_max = objid;
		}
	} while (n == 2);
	printf("Total number of requests: %d\n", nr_reqs);
	rewind(fptr);
	objid_max++;
	objid_array = (unsigned int *)malloc(sizeof(unsigned int) * objid_max);
	if (!objid_array) {
		perror("objid array allocation failed\n");
		return -1;
	}
	req_sequence = (struct req_info *)malloc(sizeof(struct req_info) * nr_reqs);
	if (!req_sequence) {
		perror("req_sequence allocation failed\n");
		ret = -1;
		goto free_objid_array;
	}
	for (i = 0; i < nr_reqs; i++) {
		fscanf(fptr, "%d\t%d\n", &req_sequence[i].objid, &req_sequence[i].rw);
	}
	fclose(fptr);

	ctx = legomem_open_context();
	if (!ctx) {
		ret = -1;
		goto free_req_sequence;
	}
	dump_legomem_contexts();

	ti = malloc(sizeof(*ti) * NR_MAX);
	if (!ti) {
		perror("thread info allocation failed\n");
		ret = -1;
		goto close_context;
	}
	tid = malloc(sizeof(*tid) * NR_MAX);
	if (!tid) {
		perror("pthread_t allocation failed\n");
		ret = -1;
		goto free_ti;
	}

	pthread_barrier_init(&thread_barrier, NULL, nr_threads);
	for (i = 0; i < nr_threads; i++) {
		/*
		 * cpu 0 is used for gbn polling now
		 * cpu 1 is used for management
		 */
		ti[i].cpu = mgmt_dispatcher_thread_cpu + 1 + i;
		ti[i].id = i;
		ti[i].seq_start_idx = i*(nr_reqs/nr_threads);
		ti[i].objid_array_start_idx = i*(objid_max/nr_threads);
		if (i == nr_threads - 1) {
			ti[i].objid_array_end_idx = objid_max - 1;
			ti[i].seq_end_idx = nr_reqs - 1;
		} else {
			ti[i].objid_array_end_idx = (i+1)*(objid_max/nr_threads) - 1;
			ti[i].seq_end_idx = (i+1)*(nr_reqs/nr_threads) - 1;
		}

		perthread_buffer[i] = (char *)malloc(data_size * 2 +
					sizeof(struct legomem_common_headers));
		if (!perthread_buffer[i]) {
			ret = -1;
			goto free_tid;
		}

		ret = pthread_create(&tid[i], NULL, thread_func, &ti[i]);
		if (ret) {
			printf("fail to create test thread");
			exit(-1);
		}
	}

	for (i = 0; i < nr_threads; i++) {
		pthread_join(tid[i], NULL);
	}

	/* compute stats */
	for (i = 0; i < nr_threads; i++) {
		nr_create += all_req_stats[i].nr_create;
		nr_delete += all_req_stats[i].nr_delete;
		nr_read += all_req_stats[i].nr_read;
		nr_write += all_req_stats[i].nr_write;

		sum_create += latency_create_ns[i];
		sum_delete += latency_delete_ns[i];
		sum_read += latency_read_ns[i];
		sum_write += latency_write_ns[i];
	}
	if (nr_create != 0)
		avg_create = sum_create / nr_create;
	if (nr_delete != 0)
		avg_delete = sum_delete / nr_delete;
	if (nr_read != 0)
		avg_read = sum_read / nr_read;
	if (nr_write != 0)
		avg_write = sum_write / nr_write;

	printf("#nr_theads=%3d #data_size=%8d "
		"avg_create_RTT: %10lf ns avg_delete_RTT: %10lf ns "
		"avg_read_RTT: %10lf ns avg_write_RTT: %10lf ns\n",
		nr_threads, data_size,
		avg_create, avg_delete, avg_read, avg_write);

free_tid:
	free(tid);
free_ti:
	free(ti);
close_context:
	legomem_close_context(ctx);
free_req_sequence:
	free(req_sequence);
free_objid_array:
	free(objid_array);
	return ret;
}
