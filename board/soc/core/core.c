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

#include "dma.h"
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
static int TW_HEAD = 0;
static int TB_HEAD = 0;
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

		vi = index_to_vregion(pi, vregion_idx);

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
 * TODO:
 *
 * 1) Notify FPGA stack to handle this session.
 * 2) Notify FPGA to free session. 
 *
 * Couple ways to implement this:
 * 1) In the fpga stack, check for close/open msgs,
 *    and act on its own.
 * 2) soc explicitly notify fpga to do sth.
 */
static void handle_open_session(struct thpool_buffer *tb)
{
	struct op_open_close_session_ret *resp;
	unsigned int session_id;

	resp = (struct op_open_close_session_ret *)resp;
	set_tb_tx_size(tb, sizeof(*resp));

	session_id = alloc_session_id();
	if (session_id < 0) {
		resp->session_id = 0;
		return;
	}

	resp->session_id = session_id;
}

static void handle_close_session(struct thpool_buffer *tb)
{
	struct op_open_close_session *req;
	struct op_open_close_session_ret *resp;
	unsigned int session_id;

	resp = (struct op_open_close_session_ret *)resp;
	set_tb_tx_size(tb, sizeof(*resp));

	session_id = req->session_id;
	free_session_id(session_id);
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

/*
 * Monitor asked us to migrate a certain vregion to a new node.
 * The new node has already been notified and waiting for us.
 *
 * TODO
 * 1. Tell fpga to stop/hold any traffic targeting this vRegion.
 *    actually we can also make sure of this at host side?
 * 2. ask fpga to migrate the data
 */
static void handle_migration_send(struct thpool_buffer *tb)
{
}

/*
 * Monitor asked us to prepare for a upcoming migration.
 * We need to create context, vregion etc, and wait for
 * the old owner to send the data.
 */
static void handle_migration_recv(struct thpool_buffer *tb)
{
}

static void handle_migration_recv_cancel(struct thpool_buffer *tb)
{
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

static void worker_handle_request_inline(struct thpool_worker *tw,
					 struct thpool_buffer *tb)
{
	struct lego_header *lego_hdr;
	uint16_t opcode;

	lego_hdr = to_lego_header(tb->rx);
	opcode = lego_hdr->opcode;

	switch (opcode) {
	/* VM */
	case OP_REQ_ALLOC:
		handle_alloc_free(tb->rx, tb->rx_size, tb, true);
		break;
	case OP_REQ_FREE:
		handle_alloc_free(tb->rx, tb->rx_size, tb, false);
		break;

	/* Proc */
	case OP_CREATE_PROC:
		handle_create_proc(tb);
		break;
	case OP_FREE_PROC:
		handle_free_proc(tb);
		break;

	case OP_REQ_MIGRATION_M2B_SEND:
		handle_migration_send(tb);
		break;
	case OP_REQ_MIGRATION_M2B_RECV:
		handle_migration_recv(tb);
		break;
	case OP_REQ_MIGRATION_M2B_RECV_CANCEL:
		handle_migration_recv_cancel(tb);
		break;

	case OP_OPEN_SESSION:
		handle_open_session(tb);
		break;
	case OP_CLOSE_SESSION:
		handle_close_session(tb);
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
		dma_send(tb->tx, tb->tx_size);
	free_thpool_buffer(tb);
}

static void dispatcher(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	size_t ret;

	while (1) {
		struct lego_header *lego_header;
		int payload_size;
		void *payload_ptr;

		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		/*
		 * FPGA does not deliver the L2-L4 and GBN headers to us.
		 * But to keep the handlers consistent, we skip the first portion
		 * of the receive buffer and reserve the space.
		 *
		 * We will first use a recv to get the headers, which has a field
		 * indicating the whole length of the packet. We then use that length
		 * to read the payload. Thus the whole process has two recv.
		 */
		lego_header = to_lego_header(tb->rx);
		ret = dma_recv_blocking(lego_header, LEGO_HEADER_SIZE);
		if (ret < 0) {
			printf("%s(): fail to recv lego_header\n", __func__);
			continue;
		}

		payload_size = lego_header->size - LEGO_HEADER_OFFSET;
		payload_ptr = (void *)(lego_header + 1);
		ret = dma_recv_blocking(payload_ptr, payload_size);
		if (ret < 0) {
			printf("%s(): fail to recv payload\n", __func__);
			continue;
		}

		/* Inline handling */
		worker_handle_request_inline(tw, tb);
	}
}

int main(int argc, char **argv)
{
	int ret;

	ret = init_dma();
	if (ret) {
		printf("Fail to init dma\n");
		return 0;
	}

	ret = init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	if (ret) {
		printf("Fail to init thpool\n");
		return 0;
	}

	ret = init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map, dma_thpool_alloc_cb);
	if (ret) {
		printf("Fail to init thpool buffer\n");
		return 0;
	}

	init_session_subsys();

	ret = init_proc_subsystem();
	if (ret) {
		printf("Fail to init proc\n");
		return ret;
	}

	dispatcher();
	return 0;
}
