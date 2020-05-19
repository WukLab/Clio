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

#define MAX_LINE_SIZE 102400

uint16_t threads_ready_cnt;
int thread_can_start;
uint64_t total_num_load_reqs;
uint64_t total_num_run_reqs;
int max_key_size;
int value_size;
int total_num_thread;
char **load_reqs;
struct ycsb_run_req_struct {
	int op;
	char *key;
};
struct ycsb_run_req_struct *run_reqs;

struct session_net **sessions;
struct legomem_context **legomem_ctx;
int total_num_boards;

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

int map_key_to_session_id(char *key, int thread_id)
{
	int board_id;

	board_id = key % total_num_boards;

	return board_id * total_num_thread + thread_id;
}

struct run_thread_input {
	int thread_id;
};

void ycsb_workload_run_phase(void *input) 
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

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < total_num_run_reqs; i++) {
		if (i % total_num_thread != my_thread_id)
			continue;

		session_id = map_key_to_session_id(run_reqs[i].key, my_thread_id);

		switch (run_reqs[i].op) {
			case UPDATE:
				legomem_kvs_update(legomem_ctx[session_id], sessions[session_id], max_key_size, run_reqs[i].key, 
						value_size, value_buf);
				break;
			case READ:
				legomem_kvs_read(legomem_ctx[session_id], sessions[session_id], max_key_size, run_reqs[i].key, 
						value_size, value_buf);
				break;
			case DELETE:
				legomem_kvs_delete(legomem_ctx[session_id], sessions[session_id], max_key_size, run_reqs[i].key); 
				break;
			default:
				printf("error op code in input file %d\n", i);
		}
		req_cnt++;
	}
	clock_gettime(CLOCK_MONOTONIC, &ts);

	printf("thread %d run time %f ns avg latency %f ns\n", my_thread_id, (te.tv_sec*1.0e9 + te.tv_nsec) - 
		(ts.tv_sec*1.0e9 + ts.tv_nsec), ((te.tv_sec*1.0e9 + te.tv_nsec) - 
		(ts.tv_sec*1.0e9 + ts.tv_nsec) / req_cnt));
	free(value_buf);
	return NULL;
}

int ycsb_workload_load_phase()
{
	int i;
	char *value_buf;
	int session_id;
	for (i = 0; i < total_num_run_reqs / total_num_thread; i++) {
		id = i / total_num_thread + my_thread_id;
		switch (run_reqs[id].op) {
=======
	for (i = 0; i < total_num_run_reqs; i++) {
		if (i % total_num_thread != my_thread_id)
			continue;
		switch (run_reqs[i].op) {
>>>>>>> tested parsing and running fake ycsb with 100K reqs
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
		session_id = map_key_to_session_id(load_reqs[i], 0);
		legomem_kvs_create(legomem_ctx[session_id], sessions[session_id], max_key_size, load_reqs[i], 
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
	char line[1024];
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
		if (strlen(key) > max_key_size)
			max_key_size = strlen(key);
		if (strlen(key) > MAX_KEY_SIZE) {
			printf("key size exceeding max %lu\n", strlen(key));
			return -1;
		}

		if (op_code == INSERT) {
			memcpy(load_reqs[load_req_count], key, strlen(key));
	while (fgets(line, sizeof(line), fd)) {
		sscanf(line, "%llu %llu\n", &op_code, &key);
		if (op_code == INSERT) {
			memcpy(load_reqs[load_req_count], key, key_size);
			load_req_count++;
		}
		else {
			run_reqs[run_req_count].op = op_code;
			memcpy(run_reqs[run_req_count].key, key, strlen(key));
			run_reqs[run_req_count].key = key;
=======
			memcpy(run_reqs[run_req_count].key, key, key_size);
>>>>>>> tested parsing and running fake ycsb with 100K reqs
			run_req_count++;
		}
	}

	total_num_load_reqs = load_req_count;
	total_num_run_reqs = run_req_count;
	printf("total total_num_load_reqs %lu total_num_run_reqs %lu max_key_size %d\n",
			total_num_load_reqs, total_num_run_reqs, max_key_size);

	fclose(fp);
	free(key);
	return max_key_size;
}

int run_ycsb_workload(char* filename, int thread_num, int input_value_size,
			uint64_t total_loads, uint64_t total_reqs)
{
	int i;
	pthread_t *thread_job;
	struct run_thread_input *thread_input;
	struct timespec ts, te;

	total_num_thread = thread_num;
	value_size = input_value_size;

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
	max_key_size = 0;
	total_num_load_reqs = total_loads;
	total_num_run_reqs = total_reqs;

	thread_input = (struct run_thread_input *)malloc(thread_num * 
			sizeof(struct run_thread_input *));
	load_reqs = (char **)malloc(total_num_load_reqs * sizeof(char *));
	run_reqs = (struct ycsb_run_req_struct *)malloc(total_num_run_reqs * 
			sizeof(struct ycsb_run_req_struct));
	assert(load_reqs && run_reqs);
	for (i = 0; i < total_num_load_reqs; i++) {
		load_reqs[i] = (char *)malloc(MAX_KEY_SIZE);
		assert(load_reqs[i]);
	}
	for (i = 0; i < total_num_run_reqs; i++) {
		run_reqs[i].key = (char *)malloc(MAX_KEY_SIZE);
		assert(run_reqs[i].key);
	}

	if (ycsb_prepare_workload_from_trace(filename) == -1)
		return -1;

	ycsb_workload_load_phase();

	clock_gettime(CLOCK_MONOTONIC, &ts);
	thread_can_start = 0;
	for (i = 0; i < thread_num; i++) {
		thread_input[i].thread_id = i;
		printf("creating thread %d\n", i);
		pthread_create(&thread_job[i], NULL, &ycsb_workload_run_phase, &thread_input[i]);
	total_num_load_reqs = 0;
	total_num_run_reqs = 0;

	//sleep(1);

	ycsb_prepare_workload_from_trace(filename);
=======
		load_reqs[i] = (char *)malloc(key_size);
		assert(load_reqs[i]);
	}
	for (i = 0; i < total_num_run_reqs; i++) {
		run_reqs[i].key = (char *)malloc(key_size);
		assert(run_reqs[i].key);
	}
>>>>>>> tested parsing and running fake ycsb with 100K reqs

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
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("total run time %f ns\n", (te.tv_sec*1.0e9 + te.tv_nsec) - 
		(ts.tv_sec*1.0e9 + ts.tv_nsec));

	free(thread_job);
	return 0;
}

int legomem_setup(int total_client_nodes, int total_boards, int num_threads)
{
	int i, j, k;
	struct board_info *board;

	sessions = (struct session_net **)malloc(total_boards * 
			num_threads * sizeof(struct session_net));
	legomem_ctx = (struct legomem_context **)malloc(total_boards * 
			num_threads * sizeof(struct legomem_context));
	if (!sessions || !legomem_ctx)
		return -1;

	total_num_boards = total_boards;

	for (i = 0; i < total_boards; i++) {
		board = find_board_by_id(i + 2);
		if (!board)
			return -1;
		for (j = 0; j < num_threads; j++) {
			k = i * num_threads + j;
			sessions[k] = legomem_open_session(legomem_ctx[k], board);
			if (!sessions[k]) {
				printf("Cannot open session to board %d thread %d\n", i, j);
				return -1;
			}
		}
	}

	return 0;
}

void run_ycsb()
{
	legomem_setup(1, 5, 2);
	//run_ycsb_workload();

	free(thread_job);
	return;
}
