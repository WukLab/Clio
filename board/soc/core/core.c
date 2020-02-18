/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "core.h"
#include "thpool.h"

static int TW_HEAD;
static struct thpool_worker *thpool_worker_map;

/* Init per worker thpool ring buffers */
static int init_thpool_buffer(struct thpool_worker *tw)
{
	int i;
	size_t buf_sz;
	struct thpool_buffer *map;

	buf_sz = sizeof(struct thpool_buffer) * NR_THPOOL_BUFFER;
	map = malloc(buf_sz);
	if (!map)
		return -ENOMEM;

	for (i = 0; i < NR_THPOOL_BUFFER; i++) {
		struct thpool_buffer *tb;

		tb = map + i;
		tb->flags = 0;
		memset(&tb->buffer, 0, THPOOL_BUFFER_SIZE);
	}

	tw->thpool_buffer_map = map;
	tw->TB_HEAD = 0;

	return 0;
}

/*
 * Allocate worker array
 * and then init per worker ring buffer array.
 */
static int init_thpool(void)
{
	int i, ret;
	size_t buf_sz;

	buf_sz = sizeof(struct thpool_worker) * NR_THPOOL_WORKERS;
	thpool_worker_map = malloc(buf_sz);
	if (!thpool_worker_map)
		return -ENOMEM;

	for (i = 0; i < NR_THPOOL_WORKERS; i++) {
		struct thpool_worker *tw;

		tw = thpool_worker_map + i;
		tw->cpu = 0;
		tw->nr_queued = 0;
		pthread_spin_init(&tw->lock, PTHREAD_PROCESS_PRIVATE);

		ret = init_thpool_buffer(tw);
		if (unlikely(ret))
			return ret;
	}

	TW_HEAD = 0;
	return 0;
}

/*
 * Select a worker to handle the next request
 * in a round-robin fashion.
 */
static __always_inline struct thpool_worker *
select_thpool_worker_rr(void)
{
	struct thpool_worker *tw;
	int idx;

	idx = TW_HEAD % NR_THPOOL_WORKERS;
	tw = thpool_worker_map + idx;
	TW_HEAD++;
	return tw;
}

/*
 * Allocate a thpool within the worker's ring buffer.
 * Since this func is called within the worker, no sync is needed.
 * However, we do need wait until the next buffer is available.
 */
static __always_inline struct thpool_buffer *
alloc_thpool_buffer(struct thpool_worker *tw)
{
	struct thpool_buffer *tb;
	int idx;

	idx = tw->TB_HEAD % NR_THPOOL_BUFFER;
	tb = tw->thpool_buffer_map + idx;
	tw->TB_HEAD++;

	/*
	 * If this happens during runtime, it means:
	 * - ring buffer is not large enough
	 * - some previous handlers are too slow
	 */
	while (unlikely(ThpoolBufferUsed(tb))) {
		;
	}

	SetThpoolBufferUsed(tb);
	barrier();
	return tb;
}

static __always_inline void
free_thpool_buffer(struct thpool_buffer *tb)
{
	tb->flags = 0;
	tb->buffer_size = 0;
	barrier();
}

static int handle_alloc_free(void *rx_buf, size_t rx_buf_size,
			     struct thpool_buffer *tb)
{
	//struct proc_info *pi;
	//struct vregion_info *vi;

	return 0;
}

/*
 * TODO:
 * Send the packet out,
 * use the tb->buffer_size and tb->buffer
 */
void tx(struct thpool_buffer *tb)
{
}

/*
 * TODO:
 * 1) assume rx_buf and rx_buf_size are already prepared.
 * 2) assume rx_buf is the whole packet which includes eth/ip/udp headers.
 *
 * This one is running within a specific thpool worker.
 */
void handle_requests(struct thpool_worker *tw, void *rx_buf, size_t rx_buf_size)
{
	struct lego_hdr *lego_hdr;
	uint16_t opcode;
	struct thpool_buffer *tb;

	lego_hdr = (struct lego_hdr *)(rx_buf + LEGO_HEADER_OFFSET);
	opcode = lego_hdr->opcode;

	tb = alloc_thpool_buffer(tw);

	switch (opcode) {
	case OP_REQ_TEST:
		break;
	case OP_REQ_ALLOC:
	case OP_REQ_FREE:
		handle_alloc_free(rx_buf, rx_buf_size, tb);
		break;
	default:
		break;
	};

	if (likely(!ThpoolBufferNoreply(tb)))
		tx(tb);

	free_thpool_buffer(tb);
}

void test_va_alloc(void)
{
	unsigned long addr;
	struct vregion_info *vi;
	struct proc_info *pi;

	pi = alloc_proc("proc_1", 123);
	if (!pi) {
		printf("fail to create the test pi\n");
		return;
	} 
	dump_procs();

	printf("From vregion 0\n");
	vi = pi->vregion + 0;
	addr = alloc_va(pi, vi, 0x1000, 0, 0);
	printf("%#lx\n", addr);
	addr = alloc_va(pi, vi, 0x1000, 0, VM_UNMAPPED_AREA_TOPDOWN);
	printf("%#lx\n", addr);

	printf("From vregion 1\n");

	vi = pi->vregion + 1;
	addr = alloc_va(pi, vi, 0x10000000, 0, VM_UNMAPPED_AREA_TOPDOWN);
	printf("1 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x10000000, 0, VM_UNMAPPED_AREA_TOPDOWN);
	printf("2 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x10000000, 0, 0);
	printf("3 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x10000000, 0, 0);
	printf("4 %#lx\n", addr);

	free_va(pi, vi, addr, 0x10000000);
	printf("free %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x2000, 0, 0);
	printf("5 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x2000, 0, 0);
	printf("6 %#lx\n", addr);
}

int main(int argc, char **argv)
{
	int ret;

	ret = init_thpool();
	if (ret) {
		printf("Fail to init thpool\n");
		return ret;
	}

	ret = init_proc_subsystem();
	if (ret) {
		printf("Fail to init proc\n");
		return ret;
	}

	test_va_alloc();

	return 0;
}
