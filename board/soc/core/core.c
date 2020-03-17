/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * This file describes the SoC side thpool workers and buffers:
 * they bridge SoC and FPGA together.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>
#include <uapi/thpool.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "core.h"

#define NR_THPOOL_WORKERS	(1)
#define NR_THPOOL_BUFFER	(32)

/*
 * Each thpool worker is described by struct thpool_worker,
 * and it is a standalone thread, running the generic handler only.
 * We can have one or multiple workers depends on config.
 *
 * The flow is:
 * a) Dispatcher allocate thpool buffer
 * b) Dispatcher receive network packet from FPGA
 * c) Dispacther find a worker, and delegate the request
 * d) The worker handles the req, send reply to FPGA, and free the thpool buffer.
 *
 * The thpool buffer is using a simple ring-based design.
 */
static int TW_HEAD;
static int TB_HEAD;
static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

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

static __always_inline struct thpool_buffer *
alloc_thpool_buffer(void)
{
	struct thpool_buffer *tb;
	int idx;

	idx = TB_HEAD % NR_THPOOL_BUFFER;
	tb = thpool_buffer_map + idx;
	TB_HEAD++;

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
	barrier();
}

static int init_thpool_buffer(void)
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
		tb->rx_size = 0;
		tb->tx_size = 0;
		memset(&tb->tx, 0, THPOOL_BUFFER_SIZE);
		memset(&tb->rx, 0, THPOOL_BUFFER_SIZE);
	}

	thpool_buffer_map = map;
	TB_HEAD = 0;
	return 0;
}

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

		if (unlikely(ret))
			return ret;
	}
	TW_HEAD = 0;
	return 0;
}

/*
 * Handle alloc/free requests from host.
 *
 * Note that
 * 1) This might be the first the host contacting with us,
 * thus there might be no context created. We will create on if that's the case.
 * 2) Upon alloc, remote will tell us the vRegion index, which, was chosen by monitor.
 * 3) We only perform allocation withi one vRegion.
 *    But we could perform free spanning multiple vRegions.
 */
static void handle_alloc_free(void *rx_buf, size_t rx_buf_size,
			      struct thpool_buffer *tb, bool is_alloc)
{
	struct proc_info *pi;
	unsigned int pid, node, host_ip;
	struct op_alloc_free *ops;
	struct op_alloc_free_ret *reply;

	/* Setup the reply buffer */
	reply = (struct op_alloc_free_ret *)tb->tx;
	set_tb_tx_size(tb, sizeof(*reply));

	ops = get_op_struct(rx_buf);

	// TODO the packet format is unknown for now.
	pid = 0;
	node = 0;
	host_ip = 0;

	pi = get_proc_by_pid(pid, node);
	if (!pi) {
		/*
		 * This could happen.
		 * Because we might be a new board assigned to the
		 * host by monitor upon allocation.
		 */
		pi = alloc_proc(pid, node, NULL, host_ip);
		if (!pi) {
			reply->ret = -ENOMEM;
			return;
		}

		/* We will drop in the end */
		get_proc_by_pid(pid, node);
	}

	if (is_alloc) {
		/* OP_REQ_ALLOC */
		unsigned long addr, len, vregion_idx, vm_flags;
		struct vregion_info *vi;

		len = ops->len;
		vregion_idx = ops->vregion_idx;
		vm_flags = ops->vm_flags;

		vi = index_to_vregion(vregion_idx);

		addr = alloc_va_vregion(pi, vi, len, vm_flags);
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
}

static void handle_create_proc(struct thpool_buffer *tb)
{
	struct proc_info *pi;
	int *reply;
	unsigned int pid, node;
	void *rx_buf;

	reply = (int *)tb->tx;
	set_tb_tx_size(tb, sizeof(int));

	/* TODO: Get PID and NODE from request buffer */
	rx_buf = tb->rx;
	pid = 1;
	node = 0;

	pi = alloc_proc(pid, node, NULL, 0);
	if (!pi) {
		*reply = -ENOMEM;
	} else
		*reply = 0;
}

static void handle_free_proc(struct thpool_buffer *tb)
{
	struct proc_info *pi;
	int *reply;
	unsigned int pid, node;
	void *rx_buf;

	reply = (int *)tb->tx;
	set_tb_tx_size(tb, sizeof(int));

	/* TODO: Get PID and NODE from request buffer */
	rx_buf = tb->rx;
	pid = 1;
	node = 0;

	pi = get_proc_by_pid(pid, node);
	if (!pi) {
		*reply = -EINVAL;
		return;
	}

	/* We grabbed one ref above, thus put twice */
	put_proc_info(pi);
	put_proc_info(pi);
}

/*
 * Handle SoC Pingpong testing request.
 * Simply return and let sender measure RTT.
 */
static int handle_soc_pingpong(struct thpool_buffer *tb)
{
	int *reply;

	reply = (int *)tb->tx;
	*reply = 0;
	set_tb_tx_size(tb, sizeof(int));
	return 0;
}

/*
 * This function resets all data structures as if the board just booted.
 * It would come handy during testing season, no need to reboot board and all.
 */
static int handle_reset_all(struct thpool_buffer *tb)
{
	return 0;
}

static int handle_migration(struct thpool_buffer *tb)
{
	return 0;
}

/*
 * This handler is a generic debug handler that could
 * handle various debugging requests
 *
 * 1) dump one proc_info based on pid
 * 2) dump all proc_info
 * 3) dump vregion info
 * 4) dump stats
 */
static int handle_soc_debug(struct thpool_buffer *tb)
{
	return 0;
}

/*
 * This is the AXI DMA interface.
 * Each direction has its own channel.
 * Nonetheless, the interface is simple enough.
 */
#if 0
int axidma_oneway_transfer(axidma_dev_t dev, int channel, void *buf,
        size_t len, bool wait);
#endif

static inline size_t axidma_soc_to_fpga(void *buf, size_t buf_size)
{
	return -ENOSYS;
}

static inline size_t axidma_fpga_to_soc(void *buf, size_t buf_size)
{
	return -ENOSYS;
}

static void worker_handle_request(struct thpool_worker *tw,
				  struct thpool_buffer *tb)
{
	struct lego_hdr *lego_hdr;
	uint16_t opcode;
	void *rx_buf;
	size_t rx_buf_size;

	rx_buf = tb->rx;
	rx_buf_size = tb->rx_size;

	lego_hdr = (struct lego_hdr *)(rx_buf + LEGO_HEADER_OFFSET);
	opcode = lego_hdr->opcode;

	switch (opcode) {
	/* VM */
	case OP_REQ_ALLOC:
		handle_alloc_free(rx_buf, rx_buf_size, tb, true);
		break;
	case OP_REQ_FREE:
		handle_alloc_free(rx_buf, rx_buf_size, tb, false);
		break;

	/* Proc */
	case OP_CREATE_PROC:
		handle_create_proc(tb);
		break;
	case OP_FREE_PROC:
		handle_free_proc(tb);
		break;

	case OP_REQ_MIGRATION:
		handle_migration(tb);
		break;

	/* Misc */
	case OP_RESET_ALL:
		handle_reset_all(tb);
		break;
	case OP_REQ_SOC_PINGPONG:
		handle_soc_pingpong(tb);
		break;
	case OP_REQ_SOC_DEBUG:
		handle_soc_debug(tb);
		break;
	default:
		break;
	};

	if (likely(!ThpoolBufferNoreply(tb)))
		axidma_fpga_to_soc(tb->tx, tb->tx_size);
	free_thpool_buffer(tb);
}

/*
 * General rules:
 * The dispatcher will allocate the whole thpool_buffer,
 * which will be passed to each worker handler, and its
 * the worker's responsibility to free the thpool buffer.
 */
static void dispatcher(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	size_t ret;

	while (1) {
		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		ret = axidma_fpga_to_soc(tb->rx, tb->rx_size);
		if (ret < 0) {
			printf("axi dma fpga to soc failed\n");
			return;
		}

		/* Inline handling for now */
		worker_handle_request(tw, tb);
	}
}

int main(int argc, char **argv)
{
	int ret;

	init_thpool();
	init_thpool_buffer();

	ret = init_proc_subsystem();
	if (ret) {
		printf("Fail to init proc\n");
		return ret;
	}

	test_vm();

	return 0;
}
