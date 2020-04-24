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
#include <uapi/lego_mem.h>
#include <fpga/lego_mem_ctrl.h>

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
 * Note that in the current implementation flow, neither host nor monitor
 * will explicitly contact the board for new proc creation. That will be
 * postponed until vRegion allocation time. This design choice simplies
 * the flow with some cost of lost security (check comment on vRegion handler).
 *
 * Thus this handler is actually not used. But keep it here and assume
 * the sender can either by host or monitor. Further, we assume the PID
 * has been allocated alreay.
 */
static void handle_create_proc(struct thpool_buffer *tb)
{
	struct legomem_create_context_req *req;
	struct legomem_create_context_resp *resp;
	struct lego_header *lego_header;
	struct proc_info *pi;
	pid_t pid;

	resp = (struct legomem_create_context_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_create_context_req *)tb->rx;
	lego_header = to_lego_header(req);

	/*
	 * @pid was allocated by monitor
	 * and it is globally unique
	 */
	pid = lego_header->pid;
	pi = alloc_proc(pid, NULL, 0);
	if (!pi) {
		resp->op.ret = -ENOMEM;
		return;
	} 

	/* Success */
	resp->op.ret = 0;
	resp->op.pid = pid;
}

static void handle_free_proc(struct thpool_buffer *tb)
{
	struct legomem_close_context_req *req;
	struct legomem_close_context_resp *resp;
	struct lego_header *lego_header;
	struct proc_info *pi;
	pid_t pid;

	resp = (struct legomem_close_context_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_close_context_req *)tb->rx;
	lego_header = to_lego_header(req);
	pid = lego_header->pid;

	pi = get_proc_by_pid(pid);
	if (!pi) {
		resp->ret = -ESRCH;
		return;
	}

	/* We grabbed one ref above, thus put twice */
	put_proc_info(pi);
	put_proc_info(pi);

	resp->ret = 0;
}

static void handle_open_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	struct lego_mem_ctrl gbn_open_req;
	unsigned int session_id, src_sesid;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	src_sesid = req->op.session_id;

	session_id = alloc_session_id();
	if (session_id < 0) {
		resp->op.session_id = 0;
		return;
	}

	/* Success */
	resp->op.session_id = session_id;

	printf("%s(): src_sesid: %u dst_sesid: %u\n",
		__func__, src_sesid, session_id);

	/* Notify GBN setup_manager */
	set_gbn_conn_req(&gbn_open_req.param32, session_id, GBN_SOC2FPGA_SET_TYPE_OPEN);
	gbn_open_req.epid = LEGOMEM_CONT_NET;
	gbn_open_req.cmd = 0;
	dma_ctrl_send(&gbn_open_req, sizeof(gbn_open_req));
}

static void handle_close_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	struct lego_mem_ctrl gbn_close_req;
	unsigned int session_id;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->rx;
	set_tb_tx_size(tb, sizeof(*resp));

	session_id = req->op.session_id;
	free_session_id(session_id);

	/* Success */
	resp->op.session_id = 0;

	/* Notify GBN setup_manager */
	set_gbn_conn_req(&gbn_close_req.param32, session_id, GBN_SOC2FPGA_SET_TYPE_CLOSE);
	gbn_close_req.epid = LEGOMEM_CONT_NET;
	gbn_close_req.cmd = 0;
	dma_ctrl_send(&gbn_close_req, sizeof(gbn_close_req));
}

/*
 * Handle SoC Pingpong testing request.
 * Simply return and let sender measure RTT.
 */
static int handle_soc_pingpong(struct thpool_buffer *tb)
{
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
 * Notes
 * 1. Monitor has made sure all hosts, boards will stop using
 *    this vRegion. Thus we do not to stop traffic at this board.
 * 2. The flow of this function is as follows:
 *    a) first we will free the local VMA resources
 *    b) then we will construct a legomem READ request to core-mem pipeline,
 *       which in turn will read data out and sent to network layer.
 */
static void handle_migration_send(struct thpool_buffer *tb)
{
	struct legomem_migration_req *req;
	struct legomem_migration_resp *resp;
	struct lego_header *lego_header;
	struct op_migration *op;
	struct proc_info *pi;
	struct vregion_info *vi;
	pid_t pid;
	struct {
		struct lego_header lego_header;
		struct op_read_write op;
	} migreq;

	/* Setup req and resp */
	req = (struct legomem_migration_req *)tb->rx;
	lego_header = to_lego_header(req);
	op = &req->op;

	resp = (struct legomem_migration_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	pid = lego_header->pid;
	pi = get_proc_by_pid(pid);
	if (!pid) {
		printf("%s(): pid %d not valid\n", __func__, pid);
		resp->op.ret = -ESRCH;
		return;
	}

	/* Free vma trees within this vRegion */
	vi = index_to_vregion(pi, op->vregion_index);
	free_va_vregion(pi, vi, vregion_index_to_va(op->vregion_index),
			vregion_index_to_va(op->vregion_index + 1) - 1);

	/*
	 * Send commands to coremem pipeline
	 * XXX: need to revisit these commands setup
	 */
	memset(&migreq, 0, sizeof(migreq));
	migreq.lego_header.pid = pid;
	migreq.lego_header.tag = 0;
	migreq.lego_header.opcode = OP_REQ_READ;
	migreq.lego_header.cont = MAKE_CONT(LEGOMEM_CONT_MEM,
					    LEGOMEM_CONT_NET,
					    LEGOMEM_CONT_NONE,
					    LEGOMEM_CONT_NONE);
	migreq.lego_header.src_sesid = 0;
	migreq.lego_header.dst_sesid = 0;
	migreq.lego_header.dest_ip = 0;

	migreq.op.va = vregion_index_to_va(op->vregion_index);
	migreq.op.size = VREGION_SIZE;

	dma_send(&migreq, sizeof(migreq));

	/* Done, success */
	resp->op.ret = 0;
	put_proc_info(pi);
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
		board_soc_handle_alloc_free(tb, true);
		break;
	case OP_REQ_FREE:
		board_soc_handle_alloc_free(tb, false);
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

		payload_size = lego_header->size - LEGO_HEADER_SIZE;
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

	dispatcher();
	return 0;
}
