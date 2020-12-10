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

typedef uint64_t remote_ptr_t;

struct linkedlist {
	uint64_t data[8];
};

#define KEYBASE (0x00000000ABCD0000ULL)
#define NR_MAX_ENTRIES (256)

int test_pointer_chasing(char *_unused)
{
	struct legomem_context *ctx;
	unsigned long __remote base_remote_va;
	size_t ll_size;
	struct linkedlist *local_ll;
	void *buf, *read_buf;
	int i, j;

	ll_size = sizeof(struct linkedlist) * NR_MAX_ENTRIES;

	read_buf = malloc(ll_size + sizeof(struct legomem_read_write_req));
	buf = malloc(ll_size + sizeof(struct legomem_read_write_req));

	local_ll = buf + sizeof(struct legomem_read_write_req);

	ctx = legomem_open_context();
	if (!ctx)
		return -1;
	dump_legomem_contexts();

	base_remote_va = legomem_alloc(ctx, ll_size, LEGOMEM_VM_FLAGS_POPULATE);
	if (!base_remote_va) {
		dprintf_ERROR("Fail to do legomem alloc %#lx\n",
			      base_remote_va);
		exit(0);
	}
	dprintf_INFO("pid %d base_remote_va %#lx sizeof(struct linkedlist) = %#lx\n",
		ctx->pid, base_remote_va, sizeof(struct linkedlist));

	/*
	 * This is perf critical path, let's register a send buf
	 * for the associated session
	 */
	struct session_net *ses;
	char *session_send_buf;
	ses = find_or_alloc_vregion_session(ctx, base_remote_va);
	BUG_ON(!ses);
	session_send_buf = malloc(4096);
	net_reg_send_buf(ses, session_send_buf, 4096);

	/*
	 * We will construct the data locally,
	 * then write the whole thing to remote.
	 */
	for (i = 0; i < NR_MAX_ENTRIES; i++) {
		struct linkedlist *p;

		p = local_ll + i;

		for (int j = 0; j < 8; j++) {
			if (i % 4 == 3)
				p->data[j] = 1;
			else
				p->data[j] = 0;
		}
	}
	legomem_write_sync(ctx, buf, base_remote_va, ll_size);
	legomem_read(ctx, buf, read_buf, base_remote_va, ll_size);

	struct linkedlist *tmp = read_buf + sizeof(struct legomem_read_write_resp);
	for (i = 0; i < NR_MAX_ENTRIES; i++) {
		struct linkedlist *p = tmp + i;
		printf("rd [%3d] key %#lx next %#lx value %#lx\n",
			i, p->key, p->next, p->value);
	}

	int chase_length[] = {4, 8, 16, 32, 64};

	//XXX
	int valuesize = 8;

#define NR_RUN_PER_TEST (10000)

	for (i = 0; i < ARRAY_SIZE(chase_length); i++) {
		int key_goal;
		double latency;
		struct timespec s, e;

		BUG_ON(chase_length[i] >= NR_MAX_ENTRIES);

		key_goal = chase_length[i] + KEYBASE;

		dprintf_INFO(" .. nr_chase = %d KEY_GOAL = %x\n",
			chase_length[i], key_goal);

		/* Run one configuration */
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < NR_RUN_PER_TEST; j++) {
			legomem_pointer_chasing(ctx,
						base_remote_va,
						0, // Sum
						chase_length[i] * sizeof(struct linkedlist), // total size
						valuesize, // unused
						0, // unused
						8, // unused
						chase_length[i] + 1, // unused
						16); // unused
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		dprintf_INFO("nr_chase: %d, avg latency: %lf ns\n",
			chase_length[i], latency / NR_RUN_PER_TEST);
	}

// Sum tests
	for (i = 0; i < ARRAY_SIZE(chase_length); i++) {
		int key_goal;
		double latency;
		struct timespec s, e;

		BUG_ON(chase_length[i] >= NR_MAX_ENTRIES);

		key_goal = chase_length[i] + KEYBASE;

		dprintf_INFO(" .. nr_chase = %d KEY_GOAL = %x\n",
			chase_length[i], key_goal);

		/* Run one configuration */
		clock_gettime(CLOCK_MONOTONIC, &s);
		for (j = 0; j < NR_RUN_PER_TEST; j++) {
			legomem_pointer_chasing(ctx,
						base_remote_va,
						1, // Sum
						chase_length[i] * sizeof(struct linkedlist), // total size
						valuesize, // unused
						0, // unused
						8, // unused
						chase_length[i] + 1, // unused
						16); // unused
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		dprintf_INFO("nr_chase: %d, avg latency: %lf ns\n",
			chase_length[i], latency / NR_RUN_PER_TEST);
	}

	return 0;
}
