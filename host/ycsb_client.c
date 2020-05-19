/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/compiler.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "core.h"
#include "api_kvs.h"

#define INSERT 0
#define READ 1
#define UPDATE 2
#define DELETE 3

uint16_t threads_ready_cnt;
int thread_can_start;
int total_num_load_reqs;
int total_num_run_reqs;
int total_num_thread;
int *load_reqs;
struct ycsb_run_req_struct {
	int op;
	int key;
};
struct ycsb_run_req_struct *run_reqs;

void gen_random_value(char *buf, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		buf[i] = (char)(i % 127);
	}
}

void ycsb_workload_run_phase(void *input) 
{
	int i, id;
	int my_thread_id;
	char *value_buf;
	int key_size, value_size;
	struct legomem_context legomem_ctx;

	my_thread_id = *(()input);
	value_buf = malloc(value_size);
	assert(value_buf);

	gen_random_value(value_buf, value_size);

	// wait for all threads to be ready
	__sync_fetch_and_add(&threads_ready_cnt, 1);
	while (thread_can_start == 0)
		;

	for (i = 0; i < total_num_run_reqs / total_num_thread; i++) {
		id = i / total_num_thread + my_thread_id;
		switch (run_reqs[id].op) {
			case UPDATE:
				legomem_kvs_update(&legomem_ctx, key_size, run_reqs[id].key, 
						value_size, value_buf);
				break;
			case READ:
				legomem_kvs_read(&legomem_ctx, key_size, run_reqs[id].key, 
						value_size, value_buf);
				break;
			case DELETE:
				legomem_kvs_delete(&legomem_ctx, key_size, run_reqs[id].key); 
				break;
			default:
				print("error op code in input file %d\n", i);
		}
	}

	free(value_buf);
	return;
}

int ycsb_workload_load_phase(int key_size, int value_size)
{
	int i;
	char *value_buf;
	struct legomem_context legomem_ctx;

	value_buf = malloc(value_size);
	assert(value_buf);

	gen_random_value(value_buf, value_size);

	for (i = 0; i < total_num_load_reqs; i++) {
		legomem_kvs_create(&legomem_ctx, key_size, load_reqs[i], 
				value_size, value_buf);
	}

	free(value_buf);
	return 0;
}

int ycsb_prepare_workload_from_trace(char* filename)
{
	char line[1024];
	FILE *fp;
	int op_code;
	int key;
	int load_req_count = 0;
	int run_req_count = 0;

	fp = fopen(filename, "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fd)) {
		sscanf(line, "%llu %llu\n", &op_code, &key);
		if (op_code == INSERT) {
			load_reqs[load_req_count] = key;
			load_req_count++;
		}
		else {
			run_reqs[run_req_count].op = op_code;
			run_reqs[run_req_count].key = key;
			run_req_count++;
		}
	}

	total_num_load_reqs = load_req_count;
	total_num_run_reqs = run_req_count;

	fclose(fp);
	return 0;
}

void run_ycsb(char* filename, int thread_num)
{
	int i;
	pthread_t *thread_job;

	total_num_thread = thread_num;
	thread_job = malloc(sizeof(pthread_t) * thread_num);
	assert(thread_job);
	threads_ready_cnt = 0;
	thread_can_start = 0;
	total_num_load_reqs = 0;
	total_num_run_reqs = 0;

	//sleep(1);

	ycsb_prepare_workload_from_trace(filename);

	load_reqs = (int*)malloc(total_num_load_reqs * sizeof(int));
	run_reqs = (ycsb_run_req_struct *)maloc(total_num_run_reqs * sizeof(ycsb_run_req_struct));
	assert(!load_reqs || !run_reqs);

	ycsb_workload_load_phase();

	thread_can_start = 0;
	for (i = 0; i < thread_num; i++) {
		pthread_create(&thread_job[i], NULL, &ycsb_workload_run_phase, &i);
	}
	while (threads_ready_cnt < thread_num)
		;
	thread_can_start = 1;

	for (i = 0; i < thread_num; i++)
	{
		pthread_join(thread_job[i], NULL);
	}

	free(thread_job);
	return;
}
