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
#define MAX_LINE_SIZE 102400

uint16_t threads_ready_cnt;
int thread_can_start;
uint64_t total_num_load_reqs;
uint64_t total_num_run_reqs;
int total_num_thread;
char **load_reqs;
struct ycsb_run_req_struct {
	int op;
	char *key;
};
struct ycsb_run_req_struct *run_reqs;

void gen_random_value(char *buf, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		buf[i] = (char)(i % 127);
	}
}

struct run_thread_input {
	int thread_id;
	int key_size;
	int value_size;
};

void *ycsb_workload_run_phase(void *input) 
{
	int i;
	int my_thread_id;
	char *value_buf;
	int key_size, value_size;
	struct legomem_context legomem_ctx;
	struct run_thread_input input_val;

	input_val = *((struct run_thread_input *)input);
	my_thread_id = input_val.thread_id;
	value_size = input_val.value_size;
	key_size = input_val.key_size;
	value_buf = malloc(value_size);
	assert(value_buf);

	gen_random_value(value_buf, value_size);

	// wait for all threads to be ready
	__sync_fetch_and_add(&threads_ready_cnt, 1);
	while (thread_can_start == 0)
		;

	for (i = 0; i < total_num_run_reqs; i++) {
		if (i % total_num_thread != my_thread_id)
			continue;
		switch (run_reqs[i].op) {
			case UPDATE:
				legomem_kvs_update(&legomem_ctx, key_size, run_reqs[i].key, 
						value_size, value_buf);
				break;
			case READ:
				legomem_kvs_read(&legomem_ctx, key_size, run_reqs[i].key, 
						value_size, value_buf);
				break;
			case DELETE:
				legomem_kvs_delete(&legomem_ctx, key_size, run_reqs[i].key); 
				break;
			default:
				printf("error op code in input file %d\n", i);
		}
	}

	free(value_buf);
	return NULL;
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

int ycsb_prepare_workload_from_trace(char* filename, int key_size)
{
	char line[MAX_LINE_SIZE];
	FILE *fp;
	int op_code;
	char *key;
	int load_req_count = 0;
	int run_req_count = 0;

	fp = fopen(filename, "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		sscanf(line, "%d %s\n", &op_code, key);
		if (op_code == INSERT) {
			memcpy(load_reqs[load_req_count], key, key_size);
			load_req_count++;
		}
		else {
			run_reqs[run_req_count].op = op_code;
			memcpy(run_reqs[run_req_count].key, key, key_size);
			run_req_count++;
		}
	}

	total_num_load_reqs = load_req_count;
	total_num_run_reqs = run_req_count;

	fclose(fp);
	free(key);
	return 0;
}

void run_ycsb(char* filename, int thread_num, int key_size, int value_size,
			uint64_t total_loads, uint64_t total_reqs)
{
	int i;
	pthread_t *thread_job;
	struct run_thread_input *thread_input;

	total_num_thread = thread_num;
	thread_job = malloc(sizeof(pthread_t) * thread_num);
	assert(thread_job);
	threads_ready_cnt = 0;
	thread_can_start = 0;
	total_num_load_reqs = total_loads;
	total_num_run_reqs = total_reqs;

	thread_input = (struct run_thread_input *)malloc(thread_num * 
			sizeof(struct run_thread_input *));
	load_reqs = (char **)malloc(total_num_load_reqs * sizeof(char *));
	run_reqs = (struct ycsb_run_req_struct *)malloc(total_num_run_reqs * 
			sizeof(struct ycsb_run_req_struct));
	assert(load_reqs && run_reqs);
	for (i = 0; i < total_num_load_reqs; i++) {
		load_reqs[i] = (char *)malloc(key_size);
		assert(load_reqs[i]);
	}
	for (i = 0; i < total_num_run_reqs; i++) {
		run_reqs[i].key = (char *)malloc(key_size);
		assert(run_reqs[i].key);
	}

	ycsb_prepare_workload_from_trace(filename, key_size);

	ycsb_workload_load_phase(key_size, value_size);

	thread_can_start = 0;
	for (i = 0; i < thread_num; i++) {
		thread_input[i].thread_id = i;
		thread_input[i].key_size = key_size;
		thread_input[i].value_size = value_size;
		printf("creating thread %d\n", i);
		pthread_create(&thread_job[i], NULL, &ycsb_workload_run_phase, &thread_input[i]);
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
