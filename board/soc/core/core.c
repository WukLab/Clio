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
#include <sys/sysinfo.h>

#include "dma.h"
#include "core.h"

#if 0
#define CONFIG_DEBUG_SOC
#endif

#ifdef CONFIG_DEBUG_SOC
#define soc_debug(fmt, ...) \
	printf("%s():%d " fmt, __func__, __LINE__, __VA_ARGS__);
#else
#define soc_debug(fmt, ...) do { } while (0)
#endif

/*
 * The largest mesage we can receive from the AXI DMA interface.
 */
#define MAX_LEGOMEM_SOC_MSG_SIZE (512)

/*
 * Knob
 *
 * This is a bit arbitrary. It really depends on the
 * incoming msg rate and the speed of each handler.
 */
#define NR_MAX_PER_WORKER_QUEUED	(8)

static int create_thread_barrier;

static int nr_online_cpus;
static int NR_THPOOL_WORKERS;
static int NR_THPOOL_BUFFER;

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
	struct legomem_open_close_session_req *req __maybe_unused;
	struct legomem_open_close_session_resp *resp;
	struct lego_mem_ctrl *gbn_open_req;
	unsigned int session_id;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	gbn_open_req = (struct lego_mem_ctrl *)tb->ctrl;
	set_tb_tx_size(tb, sizeof(*resp));

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
	/* Notify GBN setup_manager */
	set_gbn_conn_req(&gbn_open_req->param32, session_id, GBN_SOC2FPGA_SET_TYPE_OPEN);
	gbn_open_req->epid = LEGOMEM_CONT_NET;
	gbn_open_req->cmd = 0;
	dma_ctrl_send(gbn_open_req, sizeof(*gbn_open_req));

	soc_debug("Host sesid: %u SoC sesid: %u\n",
		req->op.session_id, session_id);
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

	soc_debug("Closed session_id: %d\n", session_id);
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

void handle_new_node(struct thpool_buffer *tb)
{
	struct legomem_membership_new_node_req *req;
	struct legomem_membership_new_node_resp *resp;
	struct endpoint_info *new_ei;

	/* Setup response */
	resp = (struct legomem_membership_new_node_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	resp->ret = 0;
	resp->comm_headers.lego.opcode = OP_OPEN_SESSION_RESP;
	resp->comm_headers.lego.cont = MAKE_CONT(LEGOMEM_CONT_NET,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE);

	/* Setup request */
	req = (struct legomem_membership_new_node_req *)tb->rx;
	new_ei = &req->op.ei;

	/* Sanity check */
	if (req->op.type != BOARD_INFO_FLAGS_HOST &&
	    req->op.type != BOARD_INFO_FLAGS_BOARD) {
		dprintf_ERROR("invalid type: %lu %s\n",
			req->op.type, board_info_type_str(req->op.type));
		return;
	}

	dprintf_INFO("!!new node added: %s, ip:port: %s:%d type: %s\n",
		req->op.name, new_ei->ip_str, new_ei->udp_port,
		board_info_type_str(req->op.type));
}

static void worker_handle_request_inline(struct thpool_worker *tw,
					 struct thpool_buffer *tb)
{
	struct lego_header *rx_lego_hdr, *tx_lego_hdr;
	uint16_t opcode;

	rx_lego_hdr = to_lego_header(tb->rx);
	tx_lego_hdr = to_lego_header(tb->tx);
	opcode = rx_lego_hdr->opcode;

#ifdef CONFIG_DEBUG_SOC
	if (1) {
		int cpu, node;
		getcpu(&cpu, &node);

		/* Session IDs were swapped already */
		soc_debug("Req src_sesid: %u to dst_sesid: %u Opcode: %s CPU: %d worker_%d\n",
			  rx_lego_hdr->dst_sesid, rx_lego_hdr->src_sesid,
			  legomem_opcode_str(opcode), cpu, tw->cpu);
	}
#endif

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
	case OP_REQ_MEMBERSHIP_NEW_NODE:
		handle_new_node(tb);
		break;
	default:
		if (1) {
			/* Reply anything, remote might be waiting */
			struct legomem_common_headers *p;

			tb->tx_size = sizeof(*p);
			p = (struct legomem_common_headers *)tb->tx;
			p->lego.cont = MAKE_CONT(LEGOMEM_CONT_NET, LEGOMEM_CONT_NONE,
					         LEGOMEM_CONT_NONE, LEGOMEM_CONT_NONE);
		}
		soc_debug("Unknown or unimplemented opcode %s\n", legomem_opcode_str(opcode));
		break;
	};

	/*
	 * To keep the handlers consistent, we skip the L2-L4 and GBN header
	 * and only send the portion after GBN header
	 */
	if (likely(!ThpoolBufferNoreply(tb))) {
		if (unlikely(tb->tx_size < LEGO_HEADER_OFFSET)) {
			dprintf_ERROR("TX size %u smaller than header %lu\n",
				tb->tx_size, LEGO_HEADER_OFFSET);
			tb->tx_size = LEGO_HEADER_OFFSET;
		}

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

#define dmb(opt)	asm volatile("dmb " #opt : : : "memory")
#define smp_wmb()	dmb(ishst)

static __always_inline void
enqueue_tail_thpool_worker(struct thpool_worker *worker, struct thpool_buffer *buffer)
{
	pthread_spin_lock(&worker->lock);
	list_add_tail(&buffer->next, &worker->work_head);
	/*
	 * We want to make sure the update of above list
	 * fields can be _seen_ by others before the counter
	 * is seen: because the worker thread check
	 * the counter first, then check/dequeue list.
	 *
	 * Trivial for x86 TSO, ensuring compiler order is enough.
	 * But ARM64 uses a relaxed consistency model, a bit more costly.
	 */
	smp_wmb();
	inc_queued_thpool_worker(worker);
	pthread_spin_unlock(&worker->lock);
}

static __always_inline struct thpool_buffer *
__dequeue_head_thpool_worker(struct thpool_worker *worker)
{
	struct thpool_buffer *buffer;

	buffer = list_entry(worker->work_head.next, struct thpool_buffer, next);
	list_del(&buffer->next);
	dec_queued_thpool_worker(worker);

	return buffer;
}

static void *thpool_worker_func(void *_tw)
{
	struct thpool_worker *tw;
	struct thpool_buffer *tb;
	int cpu, node;

	WRITE_ONCE(create_thread_barrier, 1);

	tw = (struct thpool_worker *)_tw;

	//pin_cpu(tw->cpu);
	getcpu(&cpu, &node);
	dprintf_INFO("Worker%d Running on CPU %2d Node %2d\n", tw->cpu, cpu, node);

	while (1) {
		while (nr_queued_thpool_worker(tw) == 0)
			;

		pthread_spin_lock(&tw->lock);
		while (!list_empty(&tw->work_head)) {
			tb = __dequeue_head_thpool_worker(tw);
			pthread_spin_unlock(&tw->lock);

			worker_handle_request_inline(tw, tb);
			pthread_spin_lock(&tw->lock);
		}
		pthread_spin_unlock(&tw->lock);
	}

	return NULL;
}

static void polling_dispather(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	struct lego_header *rx_lego_header, *tx_lego_header;

	while (1) {
		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		while (dma_recv_blocking(tb->rx, MAX_LEGOMEM_SOC_MSG_SIZE) < 0)
			;

		rx_lego_header = to_lego_header(tb->rx);
		memmove(rx_lego_header, tb->rx, (MAX_LEGOMEM_SOC_MSG_SIZE - LEGO_HEADER_OFFSET));

		/* FPGA has already swapped dst/src session IDs */
		tx_lego_header = to_lego_header(tb->tx);
		tx_lego_header->dest_ip = rx_lego_header->dest_ip;
		tx_lego_header->src_sesid = rx_lego_header->src_sesid;
		tx_lego_header->dst_sesid = rx_lego_header->dst_sesid;
	
		enqueue_tail_thpool_worker(tw, tb);
	}
}

static void polling_inline_handle(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	struct lego_header *rx_lego_header, *tx_lego_header;

	while (1) {
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
		while (dma_recv_blocking(tb->rx, MAX_LEGOMEM_SOC_MSG_SIZE) < 0)
			;

		rx_lego_header = to_lego_header(tb->rx);
		memmove(rx_lego_header, tb->rx, (MAX_LEGOMEM_SOC_MSG_SIZE - LEGO_HEADER_OFFSET));

		/* FPGA has already swapped dst/src session IDs */
		tx_lego_header = to_lego_header(tb->tx);
		tx_lego_header->dest_ip = rx_lego_header->dest_ip;
		tx_lego_header->src_sesid = rx_lego_header->src_sesid;
		tx_lego_header->dst_sesid = rx_lego_header->dst_sesid;

		/* Inline handling */
		worker_handle_request_inline(tw, tb);
	}
}

static void gather_sysinfo(void)
{
	FILE *fp;
	char line[128];
	int cpu, node;

	fp = popen("uname -a", "r");
	if (fp) {
		if (fgets(line, sizeof(line), fp))
			printf("Kernel: %s", line);
	}

	nr_online_cpus = get_nprocs();
	printf("There are %d physical CPUs, %d online CPUs.\n",
		get_nprocs_conf(), nr_online_cpus);

	getcpu(&cpu, &node);
	printf("initial running on CPU %d\n", cpu);
}

int main(int argc, char **argv)
{
	int ret;
	int i;

	gather_sysinfo();

	ret = init_dma();
	if (ret) {
		printf("Fail to init dma\n");
		return 0;
	}

	/*
	 * Run the polling thread on the last CPU.
	 *
	 * XXX: fail to pin, it stuck.
	 */
	//pin_cpu(nr_online_cpus - 1);
	NR_THPOOL_WORKERS = nr_online_cpus - 1;
	NR_THPOOL_BUFFER  = NR_THPOOL_WORKERS * NR_MAX_PER_WORKER_QUEUED;

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

	/*
	 * Either create a poll of workers and then start polling
	 * or just do inline handling.
	 */
	if (1) {
		for (i = 0; i < NR_THPOOL_WORKERS; i++) {
			struct thpool_worker *tw;
			pthread_t t;

			tw = thpool_worker_map + i;
			tw->cpu = i;

			create_thread_barrier = 0;
			ret = pthread_create(&t, NULL, thpool_worker_func, tw);
			if (ret) {
				dprintf_ERROR("Fail to create worker %d\n", i);
				exit(-1);
			}
			while (READ_ONCE(create_thread_barrier) == 0)
				;
			tw->task = t;
		}
		polling_dispather();
	} else
		polling_inline_handle();

	return 0;
}
