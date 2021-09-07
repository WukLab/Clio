/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/compiler.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include "../core.h"
#include "../api_kvs.h"

#define INSERT 0
#define READ 1
#define UPDATE 2
#define DELETE 3
#define MAX_LINE_SIZE 102400

uint16_t threads_ready_cnt;
int thread_can_start;
uint64_t total_num_load_reqs;
uint64_t total_num_run_reqs;
int max_key_size;
int value_size;
//char **load_reqs;
u64 *load_reqs;
struct ycsb_run_req_struct {
	int op;
	char *key;
};
struct ycsb_run_req_struct *run_reqs;

struct legomem_context *legomem_ctx;

static pthread_barrier_t thread_barrier;

void gen_random_value(char *buf, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		buf[i] = (char)(i % 127);
	}
}

#define NR_BOARDS (1)
#define NR_TOTAL_THREADS (8)

int nr_total_sessions = NR_TOTAL_THREADS;

struct session_net *sessions[NR_BOARDS * NR_TOTAL_THREADS]; 

double tput[NR_TOTAL_THREADS];
int NR_THREAD[] = {NR_TOTAL_THREADS};

int map_key_to_board_id(char *key)
{
	return  (unsigned long)key % NR_BOARDS;
}

/*
 * thread_id is per board
 * not a total num..
 */
int map_key_to_session_id(char *key, int thread_id)
{
	int board_id;
	int id;

	board_id = map_key_to_board_id(key);
	id = board_id * NR_TOTAL_THREADS + thread_id;
	return id;
}

struct run_thread_input {
	int thread_id;
};

void *ycsb_workload_run_phase(void *input) 
{
	int i;
	int my_thread_id;
	char *value_buf;
	struct run_thread_input input_val;
	int session_id;
	struct timespec ts, te;
	int req_cnt;

	input_val = *((struct run_thread_input *)input);
	my_thread_id = input_val.thread_id;
	value_buf = malloc(value_size);
	assert(value_buf);
	req_cnt = 0;

	gen_random_value(value_buf, value_size);

	int cpu, node;

	/* Start from CPU2 */
	pin_cpu(2+my_thread_id);
	legomem_getcpu(&cpu, &node);

	dprintf_CRIT("thread %d runs on CPU %d \n",
			my_thread_id, cpu);

#if 0
	/*
	 * Enable this if you are using multiple clients
	 */
	int _r;
	_r = pthread_barrier_wait(&thread_barrier);
	if (_r == PTHREAD_BARRIER_SERIAL_THREAD)
		legomem_dist_barrier();
	else
		__legomem_dist_barrier();
#endif

	int client_id = 0;
	int nr_clients = 1;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = my_thread_id; i < total_num_run_reqs; i += NR_TOTAL_THREADS) {
#if 1
		if ((i % nr_clients) != client_id)
			continue;
#endif
#if 1
		if ((i % NR_TOTAL_THREADS) != my_thread_id)
			continue;
#endif

		//int bid = map_key_to_board_id(run_reqs[i].key);
		int bid = my_thread_id % NR_BOARDS;

		session_id = bid * NR_TOTAL_THREADS + my_thread_id;
		//session_id = my_thread_id;

		switch (run_reqs[i].op) {
			case UPDATE:
				legomem_kvs_update(legomem_ctx, sessions[session_id], 8, run_reqs[i].key, 
						value_size, value_buf);
				break;
			case READ:
				legomem_kvs_read(legomem_ctx, sessions[session_id], 8, run_reqs[i].key, 
						value_size, value_buf);
				break;
			case DELETE:
				BUG();
				legomem_kvs_delete(legomem_ctx, sessions[session_id], 8, run_reqs[i].key); 
				break;
			default:
				printf("error op code in input file %d\n", i);
		}
		req_cnt++;
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	double lat = (te.tv_sec*1.0e9 + te.tv_nsec) - (ts.tv_sec*1.0e9 + ts.tv_nsec);
	printf("Thread %d Total: %lf ns Avg: %lf ns (#%d tests) IOPS: %lf\n",
		my_thread_id, lat, lat / req_cnt, req_cnt, req_cnt / lat * 1.0e9);

	tput[my_thread_id] = req_cnt / lat * 1.0e9;

	free(value_buf);
	return NULL;
}

int ycsb_workload_load_phase()
{
	int i, j;
	char *value_buf;
	int session_id;
	struct session_net *ses;

	value_buf = malloc(value_size);
	assert(value_buf);

	gen_random_value(value_buf, value_size);

	/*
	 * Populate keys to all boards...
	 */
	for (j = 0; j < NR_BOARDS; j++) {
		/* Get the first session with each board */
		session_id = j * NR_TOTAL_THREADS;
		ses = sessions[session_id];

		for (i = 0; i < total_num_load_reqs; i++) {
#if 1
			if (i % 500 == 0)
			dprintf_INFO("CREATE board=%d nr=%5d key=%#lx value_size %5d session_index %d ses id %d ses->board: %s\n",
				j, i, load_reqs[i], value_size, session_id, 
				get_local_session_id(ses),
				ses->board_info->name);
#endif
			legomem_kvs_create(legomem_ctx,
					   ses,
					   8, (char *)load_reqs[i], 
					   value_size, value_buf);
		}
	}

	free(value_buf);
	return 0;
}

int ycsb_prepare_workload_from_trace(char* filename)
{
	char line[MAX_LINE_SIZE];
	FILE *fp;
	int op_code;
	int load_req_count = 0;
	int run_req_count = 0;
	u64 key;

	fp = fopen(filename, "r");
	if (!fp) {
		dprintf_ERROR("File %s not found\n", filename);
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		sscanf(line, "%d %lu\n", &op_code, &key);

#if 0
		if (strlen(key) > max_key_size)
			max_key_size = strlen(key);
		if (strlen(key) > MAX_KEY_SIZE) {
			printf("key size exceeding max %lu\n", strlen(key));
			return -1;
		}
#endif

		if (op_code == INSERT) {
			/* Overload? */
			if (load_req_count >= total_num_load_reqs)
				continue;

			//memcpy(load_reqs[load_req_count], key, strlen(key));
			load_reqs[load_req_count] = key;
			load_req_count++;
		} else {
			/* Overload? */
			if (run_req_count >= total_num_run_reqs)
				continue;

			run_reqs[run_req_count].op = op_code;

			//memcpy(run_reqs[run_req_count].key, key, strlen(key));
			run_reqs[run_req_count].key = (char *)key;
			run_req_count++;
		}
	}

	total_num_load_reqs = load_req_count;
	total_num_run_reqs = run_req_count;

	dprintf_CRIT("total total_num_load_reqs %lu total_num_run_reqs %lu max_key_size %d\n",
			total_num_load_reqs, total_num_run_reqs, max_key_size);

	fclose(fp);
	return max_key_size;
}

/*
 *@thread_num: all
 */
int run_ycsb_workload(char* filename, int thread_num, int input_value_size,
			uint64_t total_loads, uint64_t total_reqs)
{
	int i;
	pthread_t *thread_job;
	struct run_thread_input *thread_input;

	thread_num = NR_TOTAL_THREADS;

	value_size = input_value_size;
	thread_job = malloc(sizeof(pthread_t) * thread_num);
	assert(thread_job);
	threads_ready_cnt = 0;
	thread_can_start = 0;
	max_key_size = 0;

	total_num_load_reqs = total_loads;
	total_num_run_reqs = total_reqs;

	thread_input = (struct run_thread_input *)malloc(thread_num * 
			sizeof(struct run_thread_input *));
	run_reqs = (struct ycsb_run_req_struct *)malloc(total_num_run_reqs * 
			sizeof(struct ycsb_run_req_struct));

	load_reqs = malloc(total_num_load_reqs * sizeof(*load_reqs));
	assert(load_reqs && run_reqs);

	/*
	 * Load stuff from file
	 */
	if (ycsb_prepare_workload_from_trace(filename) == -1)
		return -1;

#if 1
	/*
	 * Write to board
	 * create keys
	 */
	dprintf_CRIT("Loading (create) phase started, wait .. %d\n", 0);
	ycsb_workload_load_phase();
	dprintf_CRIT("Loading (create) phase finished ..%d\n", 0);
#endif

	double tput_sum;

	pthread_barrier_init(&thread_barrier, NULL, thread_num);

	for (i = 0; i < thread_num; i++) {
		thread_input[i].thread_id = i;
		pthread_create(&thread_job[i], NULL, &ycsb_workload_run_phase, &thread_input[i]);
	}

	for (i = 0; i < thread_num; i++) {
		pthread_join(thread_job[i], NULL);
	}

	tput_sum = 0;
	for (i = 0; i < thread_num; i++) {
		tput_sum += tput[i];
	}

	// print in red
	dprintf_ERROR("nr_threads=%d Sum IOPS %lf\n", thread_num, tput_sum);

	return 0;
}

/*
 * @num_threads is for per board.
 */
int legomem_setup(void)
{
	int i, j, k;
	struct board_info *board;
	void *buf;

	/* legomem_ctx = legomem_open_context(); */
	legomem_ctx = (struct legomem_context *)malloc(sizeof(*legomem_ctx));
	legomem_ctx->pid = 1;

	for (i = 0; i < NR_BOARDS; i++) {

		/*
		 * XXX
		 * tune the board id starting addr
		 */
		board = find_board_by_id(i + 3);
		if (!board)
			return -1;

		dprintf_CRIT("Selected Board:  [%s] \n", board->name);

		for (j = 0; j < NR_TOTAL_THREADS; j++) {
			k = i * NR_TOTAL_THREADS + j;

			//sessions[k] = legomem_open_session(legomem_ctx, board);

			sessions[k] = legomem_open_session_remote_mgmt(board);
			if (!sessions[k]) {
				printf("Cannot open session to board %d thread %d\n", i, j);
				return -1;
			}

#if 1
			/* Create per-session send RDMA buf */
			buf = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
			if (buf == MAP_FAILED) {
				printf("fail to mmap");
				exit(0);
			}
			net_reg_send_buf(sessions[k], buf, 4096);

			dprintf_CRIT("\tCreated session %d with board %s\n", k, board->name);
#endif
		}
	}

	for (i = 0; i < NR_TOTAL_THREADS * NR_BOARDS; i++) {
		dprintf_INFO(" YCSB session index %2d sesid %2d remote %s\n",
			i, get_local_session_id(sessions[i]),
			sessions[i]->board_info->name);
	}
	return 0;
}

int test_run_ycsb(char *unused)
{
	/* char *fname = "test/ycsb_datasets/workloada_parsed"; */
	/* char *fname = "test/ycsb_datasets/workloadb_parsed"; */
	char *fname = "test/ycsb_datasets/workloadc_parsed";

	int nr_boards, nr_threads;
	int value_size;

	nr_threads = NR_TOTAL_THREADS;

	nr_boards = NR_BOARDS;

	dprintf_CRIT("Start running YCSB... File: %s. nr_boards %d nr_total_threads %d\n",
		fname, NR_BOARDS, NR_TOTAL_THREADS);

	legomem_setup();

	value_size = 1000;

	/* run_ycsb_workload(fname, NR_TOTAL_THREADS, value_size, 2, 100 * 1000); */

	/*
	 * This is the one used for both Clio and SuperNIC Eval.
	 */
	run_ycsb_workload(fname, NR_TOTAL_THREADS, value_size, 100 * 1000, 100 * 1000);

	return 0;
}
