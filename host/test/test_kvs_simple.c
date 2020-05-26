/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../core.h"
#include "../api_kvs.h"

#define NR_READ		10000
#define NR_WRITE	10000

int test_kvs_simple(char *_unused)
{
	struct legomem_context *ctx;
	struct board_info *board;
	struct session_net *ses;

	dprintf_CRIT("\n\n\tRunning KVS Simple test... %d\n",0);

	ctx = legomem_open_context();
	if (!ctx) {
		dprintf_ERROR("Fail to open contxt %d\n", 0);
		return -EPERM;
	}

	board = find_board_by_id(3);
	if (!board) {
		printf("No board\n");
		exit(0);
	}
	dprintf_CRIT("Using board: %s\n", board->name);

	ses = legomem_open_session_remote_mgmt(board);
	if (!ses) {
		printf("fail top open session\n");
		exit(0);
	}
	void *reg_buf = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (reg_buf == MAP_FAILED) {
		printf("fail to mmap");
		exit(0);
	}
	net_reg_send_buf(ses, reg_buf, 4096);

	char buf[1024];
	uint64_t base_key = 0x223456789ULL;
	uint64_t key;

	int value_size = 1000;

	key = base_key;

#if 0
	legomem_kvs_create(ctx, ses, 8, (char *)key, value_size, buf);
	printf("after create..\n");
	sleep(5);

	legomem_kvs_read(ctx, ses, 8, (char *)key, value_size, buf);
	printf("after read..\n");
#endif


#if 1
	struct timespec ts, te;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (int i = 0; i < NR_WRITE; i++) {
		key = base_key + i * 0x1;

		legomem_kvs_create(ctx, ses, 8, (char *)key, value_size, buf);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("write avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_WRITE, NR_WRITE);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (int i = 0; i < NR_READ; i++) {
		key = base_key + i * 0x1;

		legomem_kvs_read(ctx, ses, 8, (char *)key, value_size, buf);
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("read avg %f ns (#%d tests)\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) -
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / NR_READ, NR_READ);
#endif

	/*
	 * End KVS Test
	 */

	legomem_close_context(ctx);
	exit(0);
	return 0;
}
