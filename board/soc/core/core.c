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

#define CONFIG_DEBUG_SOC

#ifdef CONFIG_DEBUG_SOC
#define soc_debug(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__);
#else
#define soc_debug(fmt, ...) do { } while (0)
#endif

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
	resp->comm_headers.lego.opcode = OP_CREATE_PROC_RESP;
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
	resp->comm_headers.lego.opcode = OP_FREE_PROC_RESP;
}

static void handle_open_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	struct lego_mem_ctrl *gbn_open_req;
	unsigned int session_id, src_sesid;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	gbn_open_req = (struct lego_mem_ctrl *)tb->ctrl;
	set_tb_tx_size(tb, sizeof(*resp));

	src_sesid = req->op.session_id;

	session_id = alloc_session_id();
	if (session_id < 0) {
		resp->op.session_id = 0;
		return;
	}

	/* Success */
	resp->op.session_id = session_id;
	resp->comm_headers.lego.opcode = OP_OPEN_SESSION_RESP;
	resp->comm_headers.lego.cont = MAKE_CONT(LEGOMEM_CONT_NET,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE);

	printf("%s(): host sesid: %u soc sesid: %u\n",
		__func__, src_sesid, session_id);

	/* Notify GBN setup_manager */
	set_gbn_conn_req(&gbn_open_req->param32, session_id, GBN_SOC2FPGA_SET_TYPE_OPEN);
	gbn_open_req->epid = LEGOMEM_CONT_NET;
	gbn_open_req->cmd = 0;
	dma_ctrl_send(gbn_open_req, sizeof(*gbn_open_req));
}

static void handle_close_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	struct lego_mem_ctrl *gbn_close_req;
	unsigned int session_id;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	gbn_close_req = (struct lego_mem_ctrl *)tb->ctrl;
	set_tb_tx_size(tb, sizeof(*resp));

	session_id = req->op.session_id;
	free_session_id(session_id);

	/* Success */
	resp->op.session_id = 0;
	resp->comm_headers.lego.opcode = OP_CLOSE_SESSION_RESP;
	resp->comm_headers.lego.cont = MAKE_CONT(LEGOMEM_CONT_NET,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE);

	/* Notify GBN setup_manager */
	set_gbn_conn_req(&gbn_close_req->param32, session_id, GBN_SOC2FPGA_SET_TYPE_CLOSE);
	gbn_close_req->epid = LEGOMEM_CONT_NET;
	gbn_close_req->cmd = 0;
	dma_ctrl_send(gbn_close_req, sizeof(*gbn_close_req));
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
	struct lego_header *rx_lego_hdr, *tx_lego_hdr;
	uint16_t opcode;

	rx_lego_hdr = to_lego_header(tb->rx);
	tx_lego_hdr = to_lego_header(tb->tx);
	opcode = rx_lego_hdr->opcode;

	soc_debug("%s\n", legomem_opcode_str(opcode));

	switch (opcode) {
	/* VM */
	case OP_REQ_ALLOC:
		board_soc_handle_alloc_free(tb, true);
		break;
	case OP_REQ_FREE:
		board_soc_handle_alloc_free(tb, false);
		break;

	/* Migration */
	case OP_REQ_MIGRATION_M2B_SEND:
		board_soc_handle_migration_send(tb);
		break;
	case OP_REQ_MIGRATION_M2B_RECV:
		board_soc_handle_migration_recv(tb);
		break;
	case OP_REQ_MIGRATION_M2B_RECV_CANCEL:
		board_soc_handle_migration_recv_cancel(tb);
		break;

	/* Proc */
	case OP_CREATE_PROC:
		handle_create_proc(tb);
		break;
	case OP_FREE_PROC:
		handle_free_proc(tb);
		break;

	/* Network */
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

	/*
	 * To keep the handlers consistent, we skip the L2-L4 and GBN header
	 * and only send the portion after GBN header
	 */
	if (likely(!ThpoolBufferNoreply(tb) && tb->tx_size > LEGO_HEADER_OFFSET)) {
		/*
		 * set the size field in lego header and
		 * move data to the beginning of tx buffer
		 */

		tx_lego_hdr->size = tb->tx_size - LEGO_HEADER_OFFSET;
		memmove(tb->tx, tx_lego_hdr, tx_lego_hdr->size);
		dma_send(tb->tx, tb->tx_size - LEGO_HEADER_OFFSET);
	}
	free_thpool_buffer(tb);
}

#define MAX_LEGOMEM_SOC_MSG_SIZE (512)

static void dispatcher(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	int ret;

	while (1) {
		struct lego_header *rx_lego_header, *tx_lego_header;

		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		/*
		 * FPGA does not deliver the L2-L4 and GBN headers to us.
		 * But to keep the handlers consistent, we skip the first portion
		 * of the receive buffer and reserve the space.
		 * 
		 * In theory, we should just use the lego_header ptr to receive data.
		 * But the AXI DMA IP has a limitation: the soc buffer must be 64B aligned.
		 * We cannot always guarantee the lego_header is 64B aligned due to all the
		 * headers before it. Thus we use tb->rx, which we can make sure it is aligned.
		 * This explains why we use an extra memmove at the end.
		 *
		 * We will receive the whole message in one recv. Thus we need
		 * to ensure that the receive size is larger than the largest
		 * possible request so that we won't end up receiving an
		 * imcomplete message.
		 */
		rx_lego_header = to_lego_header(tb->rx);
		rx_lego_header->opcode = 0;

		ret = dma_recv_blocking(tb->rx, MAX_LEGOMEM_SOC_MSG_SIZE);
		if (ret < 0) {
			/* if timeout or fail, skip processing the packet */
			free_thpool_buffer(tb);
			continue;
		}
		memmove(rx_lego_header, tb->rx, MAX_LEGOMEM_SOC_MSG_SIZE);

		tx_lego_header = to_lego_header(tb->tx);
		tx_lego_header->dest_ip = rx_lego_header->dest_ip;
		tx_lego_header->src_sesid = rx_lego_header->src_sesid;
		tx_lego_header->dst_sesid = rx_lego_header->dst_sesid;

		soc_debug("req from src_sesid: %u to dst_sesid: %u\n",
			  rx_lego_header->src_sesid,
			  rx_lego_header->dst_sesid);

		/* Inline handling */
		worker_handle_request_inline(tw, tb);
	}
}

int main(int argc, char **argv)
{
	int ret;
	FILE *fp;
	char line[128];

	/*
	 * Print the system info
	 * Just make sure we are running on top of the right system.
	 */
	fp = popen("uname -a", "r");
	if (!fp) {
		perror("popen uname");
		return -1;
	}
	while (fgets(line, sizeof(line), fp))
		printf("System Info: %s", line);

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
