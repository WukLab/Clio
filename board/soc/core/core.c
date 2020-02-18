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
			     struct thpool_buffer *tb, bool is_alloc)
{
	struct proc_info *pi;
	unsigned int pid;
	struct op_alloc_free *ops;
	struct op_alloc_free_ret *reply;

	/* Setup the reply buffer */
	reply = (struct op_alloc_free_ret *)tb->buffer;
	set_tb_buffer_size(tb, sizeof(*reply));

	ops = get_op_struct(rx_buf);
	pid = ops->pid;

	pi = get_proc_by_pid(pid);
	if (unlikely(!pi)) {
		printf("WARN: invalid pid %d\n", pid);
		reply->ret = -EINVAL;
		return -EINVAL;
	}

	if (is_alloc) {
		/* OP_REQ_ALLOC */
		unsigned long addr, len, vm_flags;

		len = ops->len;
		vm_flags = ops->vm_flags;
		addr = alloc_va(pi, len, vm_flags);
		if (unlikely(IS_ERR_VALUE(addr)))
			reply->ret = -ENOMEM;
		else {
			reply->ret = 0;
			reply->addr = addr;
		}
	} else {
		/* OP_REQ_FREE */
		unsigned long start, len;

		start = ops->addr;
		len = ops->len;
		reply->ret = free_va(pi, start, len);
	}

	put_proc_info(pi);
	return 0;
}

/*
 * Handle SoC Pingpong testing request.
 * Simply return and let sender measure RTT.
 */
static int handle_soc_pingpong(void *rx_buf, size_t rx_buf_size,
			       struct thpool_buffer *tb)
{
	int *reply;

	reply = (int *)tb->buffer;
	*reply = 0;
	set_tb_buffer_size(tb, sizeof(int));
	return 0;
}

static int handle_migration(void *rx_buf, size_t rx_buf_size,
			    struct thpool_buffer *tb)
{
	return 0;
}

/*
 * This handler is a generic debug handler that could
 * handle various debugging requests
 *
 * TODO:
 * 1) dump one proc_info based on pid
 * 2) dump all proc_info
 * 3) dump vregion info
 * 4) dump stats
 */
static int handle_soc_debug(void *rx_buf, size_t rx_buf_size,
			    struct thpool_buffer *tb)
{
	return 0;
}

/*
 * TODO:
 * Send the packet out,
 * use the tb->buffer_size and tb->buffer
 */
void net_send(struct thpool_buffer *tb)
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

	/*
	 * NOTE:
	 * - Each worker has its own thpool ring buffer
	 * - Handlers do NOT need to manage any RX/TX buffers
	 * - Handlers MUST NOT free the buffer
	 * - Handlers MUST NOT send net requests, this func will do so.
	 */
	tb = alloc_thpool_buffer(tw);

	switch (opcode) {
	case OP_REQ_ALLOC:
		handle_alloc_free(rx_buf, rx_buf_size, tb, true);
		break;
	case OP_REQ_FREE:
		handle_alloc_free(rx_buf, rx_buf_size, tb, false);
		break;
	case OP_REQ_MIGRATION:
		handle_migration(rx_buf, rx_buf_size, tb);
		break;
	case OP_REQ_SOC_PINGPONG:
		handle_soc_pingpong(rx_buf, rx_buf_size, tb);
		break;
	case OP_REQ_SOC_DEBUG:
		handle_soc_debug(rx_buf, rx_buf_size, tb);
		break;
	default:
		break;
	};

	/* Send reply if needed */
	if (likely(!ThpoolBufferNoreply(tb)))
		net_send(tb);

	free_thpool_buffer(tb);
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

	test_vm();

	return 0;
}
