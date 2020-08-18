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
	uint64_t key;
	remote_ptr_t value;
	remote_ptr_t next;
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
		p->key = KEYBASE + i;

		if (i < (NR_MAX_ENTRIES - 1)) {
			/*
			 * Point to next element in the array.
			 * Convert to using remote addr as well.
			 */
			p->next = (remote_ptr_t)(
				base_remote_va +
				(i + 1) * sizeof(struct linkedlist));
		} else
			p->next = 0;

		// XXX
		// if the value is used as 8B value, this is (complex) ptr chasing,
		// if this value is used as pointer to somewhere else, this is linked-list chasing
		//
		// For now just save the index.
		p->value = (remote_ptr_t)base_remote_va;

		printf("wr [%3d] key %#lx next %#lx value %#lx\n",
			i, p->key, p->next, p->value);
	}
	legomem_write_sync(ctx, buf, base_remote_va, ll_size);
	legomem_read(ctx, buf, read_buf, base_remote_va, ll_size);

	struct linkedlist *tmp = read_buf + sizeof(struct legomem_read_write_resp);
	for (i = 0; i < NR_MAX_ENTRIES; i++) {
		struct linkedlist *p = tmp + i;
		printf("rd [%3d] key %#lx next %#lx value %#lx\n",
			i, p->key, p->next, p->value);
	}

	int chase_length[] = {0, 1, 4, 16, 32, 64, 128};

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
						key_goal, // Find the last entry
						sizeof(struct linkedlist), // uint16_t structSize, size of the struct in bytes
						valuesize, // uint16_t valueSize, size of value
						0, // uint8_t keyOffset, offset of key in struct, in bytes
						8, // uint8_t valueOffset, offset of value
						chase_length[i] + 1, // uint8_t depth, not used
						16); // uint8_t nextOffset, offset of next, inbytes
		}
		clock_gettime(CLOCK_MONOTONIC, &e);

		latency = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
		dprintf_INFO("nr_chase: %d, avg latency: %lf ns\n",
			chase_length[i], latency / NR_RUN_PER_TEST);
	}

	return 0;
}
