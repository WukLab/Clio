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
#include <sys/mman.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>

#include "dma.h"
#include "core.h"
#include "pgtable.h"

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

static __always_inline void make_cont(struct thpool_buffer *tb)
{
	struct legomem_common_headers *p;

	p = (struct legomem_common_headers *)tb->tx;
	p->lego.cont = MAKE_CONT(LEGOMEM_CONT_NET, LEGOMEM_CONT_NONE,
				 LEGOMEM_CONT_NONE, LEGOMEM_CONT_NONE);
}

/*
 * Open a network session with remote.
 *
 * The steps are similar to handle_open_session.
 * 1. alloc a new local session
 * 2. notify fpga setup_manager to open it
 * 3. send a msg to remote to open a new session,
 *    we use session 0 to send this msg.
 *
 * 192.168.@ip3.@ip4
 */
struct session_net *
open_session_with_remote(unsigned char ip3, unsigned char ip4,
			 unsigned int udp_port)
{
	struct session_net *ses;
	unsigned int local_sesid;
	struct lego_mem_ctrl *ctrl;
	struct __packed {
		struct lego_header lego;
		struct op_open_close_session op;
	} *req;
	struct lego_header *lego;

	ses = alloc_session();
	if (!ses) {
		dprintf_ERROR("Fail to alloc a local session. %d\n", 0);
		return NULL;
	}
	local_sesid = get_local_session_id(ses);
	ses->board_ip = 192 << 24 | 168 << 16 | ip3 << 8 | ip4;
	ses->udp_port = udp_port;

	/* notify GBN setup_manager */
	ctrl = axidma_malloc(legomem_dma_info.dev, THPOOL_BUFFER_SIZE);
	set_gbn_conn_req(&ctrl->param32, local_sesid,
			 GBN_SOC2FPGA_SET_TYPE_OPEN);
	ctrl->epid = LEGOMEM_CONT_NET;
	ctrl->cmd = 0;
	if (dma_ctrl_send(ctrl, sizeof(*ctrl)) < 0) {
		dprintf_ERROR("fail to do dma ctrl send%d\n", 0);
		return NULL;
	}

	dprintf_INFO("after alloc session %d\n", local_sesid);

	/* Send open request to remote */ 
	req = axidma_malloc(legomem_dma_info.dev, THPOOL_BUFFER_SIZE);
	lego = &req->lego;
	lego->opcode = OP_OPEN_SESSION;
	lego->size = sizeof(*req);
	lego->src_sesid = local_sesid;
	lego->dst_sesid = 0;
	lego->dest_ip = ip3 << 24 | ip4 << 16 | (u16)(udp_port);
	lego->cont = MAKE_CONT(LEGOMEM_CONT_NET, LEGOMEM_CONT_NONE,
			       LEGOMEM_CONT_NONE, LEGOMEM_CONT_NONE);
	if (dma_send(req, sizeof(*req)) < 0) {
		dprintf_ERROR("fail to do dma data send %d\n", 0);
		return NULL;
	}

	dprintf_INFO("!after sent dma 192.168.%d.%d req %#lx size %#lx\n",
		ip3, ip4, (u64)req, sizeof(*req));

	//set_remote_session_id(ses, req->op.session_id);
	return ses;
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

/*
 * This is a two-step function.
 * First we allocate a local session struct and an ID.
 * Then we notify FPGA GBN stack to open this new session.
 */
static void handle_open_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req __maybe_unused;
	struct legomem_open_close_session_resp *resp;
	struct lego_mem_ctrl *gbn_open_req;
	struct session_net *ses;
	unsigned int session_id;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	gbn_open_req = (struct lego_mem_ctrl *)tb->ctrl;
	set_tb_tx_size(tb, sizeof(*resp));

	ses = alloc_session();
	if (!ses) {
		dprintf_ERROR("Fail to alloc a new session. %d\n", 0);
		resp->op.session_id = 0;
		return;
	}
	session_id = get_local_session_id(ses);

	/* Try to fill some info into the new ses */
	set_remote_session_id(ses, req->op.session_id);
	ses->board_ip = to_lego_header(req)->dest_ip;
	ses->udp_port = 0;

	/* Then notify GBN setup_manager() */
	set_gbn_conn_req(&gbn_open_req->param32, session_id,
			 GBN_SOC2FPGA_SET_TYPE_OPEN);
	gbn_open_req->epid = LEGOMEM_CONT_NET;
	gbn_open_req->cmd = 0;
	dma_ctrl_send(gbn_open_req, sizeof(*gbn_open_req));

	/* Setup resp */
	resp->op.session_id = session_id;
	resp->comm_headers.lego.opcode = OP_OPEN_SESSION_RESP;

	dprintf_DEBUG("new session: remote_sesid: %u local_sesid: %u\n",
			req->op.session_id, session_id);
	dump_net_sessions();
}

/*
 * Similar to handle_open_session, this is also a two-step func.
 * We first free the session and then notify FPGA GBN.
 */
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
	free_session_by_id(session_id);

	/* Success */
	resp->op.session_id = 0;
	resp->comm_headers.lego.opcode = OP_CLOSE_SESSION_RESP;

	/* Notify GBN setup_manager */
	set_gbn_conn_req(&gbn_close_req->param32, session_id,
			 GBN_SOC2FPGA_SET_TYPE_CLOSE);
	gbn_close_req->epid = LEGOMEM_CONT_NET;
	gbn_close_req->cmd = 0;
	dma_ctrl_send(gbn_close_req, sizeof(*gbn_close_req));

	dprintf_DEBUG("Closed local_sesid: %d\n", session_id);
}

/*
 * Handle SoC Pingpong testing request.
 * Simply return and let sender measure RTT.
 */
static void handle_soc_pingpong(struct thpool_buffer *tb)
{
	struct legomem_pingpong_resp *resp;

	resp = (struct legomem_pingpong_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
	resp->comm_headers.lego.opcode = OP_REQ_SOC_PINGPONG_RESP;
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
	struct board_info *bi;

	/* Setup response */
	resp = (struct legomem_membership_new_node_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	resp->ret = 0;
	resp->comm_headers.lego.opcode = OP_OPEN_SESSION_RESP;

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

	/* Finally add the board to the system */
	bi = add_board(req->op.name, new_ei->ip, new_ei->udp_port);
	if (!bi)
		return;
	bi->flags = req->op.type;

	dump_boards();
}

static void handle_query_stat(struct thpool_buffer *tb)
{
	struct legomem_query_stat_resp *resp;
	size_t size;

	/*
	 * calculate the size of resp msg
	 * minus 1 because of the original pointer
	 */
	resp = (struct legomem_query_stat_resp *)tb->tx;
	resp->comm_headers.lego.opcode = OP_REQ_QUERY_STAT_RESP;

	size = sizeof(*resp) + (NR_STAT_TYPES - 1) * sizeof(unsigned long);
	set_tb_tx_size(tb, size);

	memset(resp->stat, 0, NR_STAT_TYPES * sizeof(unsigned long));
	resp->nr_items = NR_STAT_TYPES;
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
		legomem_getcpu(&cpu, &node);

		/* Session IDs were swapped already */
		dprintf_DEBUG("Req src_sesid: %u to dst_sesid: %u Opcode: %s CPU: %d worker_%d\n",
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
	case OP_REQ_TEST_PTE:
		handle_test_pte(tb);
		break;
	case OP_REQ_QUERY_STAT:
		handle_query_stat(tb);
		break;
	default:
		if (1) {
			struct legomem_common_headers *p;
			tb->tx_size = sizeof(*p);
			make_cont(tb);
		}
		dprintf_ERROR("Unknown or unimplemented opcode %s\n", legomem_opcode_str(opcode));
		break;
	};

#ifdef CONFIG_DEBUG_SOC
	if (1) {
		int cpu, node;
		legomem_getcpu(&cpu, &node);

		/* Session IDs were swapped already */
		dprintf_DEBUG("Req src_sesid: %u to dst_sesid: %u Opcode: %s CPU: %d worker_%d DONE!\n",
			  rx_lego_hdr->dst_sesid, rx_lego_hdr->src_sesid,
			  legomem_opcode_str(opcode), cpu, tw->cpu);
	}
#endif

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
		 * Post-cooking:
		 * 1. Set cont
		 * 2. Set the size field in lego header
		 * 3. move data to the beginning of tx buffer (64B aligned)
		 *
		 * Some may wish to reply to on-board modules and may
		 * supply their own CONT, thus add this checking and skip.
		 */
		if (!(tb->flags & THPOOL_BUFFER_NOCONT))
			make_cont(tb);

		tx_lego_hdr->size = tb->tx_size - LEGO_HEADER_OFFSET;
		memmove(tb->tx, tx_lego_hdr, tx_lego_hdr->size);
		dma_send(tb->tx, tb->tx_size - LEGO_HEADER_OFFSET);
	}
	free_thpool_buffer(tb);
}

#ifdef __aarch64__
#define dmb(opt)	asm volatile("dmb " #opt : : : "memory")
#define smp_wmb()	dmb(ishst)
#else
#define smp_wmb()	barrier();
#endif

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
	legomem_getcpu(&cpu, &node);
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
	int cpu, node;

	legomem_getcpu(&cpu, &node);
	dprintf_INFO("DATA AIXS Polling Thread running on CPU %2d Node %2d\n",
		cpu, node);

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
	int cpu, node;

	legomem_getcpu(&cpu, &node);
	dprintf_INFO("DATA AIXS Polling Thread running on CPU %2d Node %2d\n",
		cpu, node);

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
			dprintf_INFO("Kernel: %s", line);
	}

	nr_online_cpus = get_nprocs();
	dprintf_INFO("There are %d physical CPUs, %d online CPUs.\n",
		get_nprocs_conf(), nr_online_cpus);

	legomem_getcpu(&cpu, &node);
	dprintf_INFO("Initial thread running on CPU %d\n", cpu);
}

/*
 * devmem is used by SoC code to access various things.
 * Here, we should have a clear separation about physical address and bus address.
 * Think all the addresses as bus address, and only a portion of it is used
 * for SoC DRAM physical address space.
 *
 * Some bus addresses (such as the ones used for dynamic IP/MAC setup) may look
 * like SoC DRAM physical address, but they are bus address that maps to FPGA registers.
 * Same for FPGA DRAM, whose bus base is FPGA_MEMORY_MAP_DATA_START (0x500000000).
 * All those things are set by Vivado Address Editor.
 * Interesting embedded systems.
 */
int devmem_fd;

void open_devmem(void)
{
	devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (devmem_fd < 0) {
		perror("open");
		dprintf_ERROR("Fail to open /dev/mem ret %d\n", errno);
		exit(1);
	}
	dprintf_INFO("Open /dev/mem fd=%d\n", devmem_fd);
}

int main(int argc, char **argv)
{
	int ret;
	int i;

	gather_sysinfo();

#if 0
	test_vm();

	open_devmem();

	ret = init_dma();
	if (ret) {
		printf("Fail to init dma\n");
		return 0;
	}
	init_stat_mapping();
	init_migration_setup();
	init_tlbflush_setup();
#endif

	/* Init buddy allocator for FPGA physical memory. */
	init_buddy();

	init_fpga_pgtable();
	init_shadow_pgtable();

#if 1
	test_vm_conflict();
	exit(0);
#endif

	/*
	 * Init all FREEPAGE fifos.
	 * Push down initial free pages.
	 */
	init_freepage_fifo();

	/*
	 * Launch a polling thread for CTRL FIFOs.
	 */
	init_ctrl_polling();


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

	//open_session_with_remote(1, 3, 8888);

	/*
	 * Either create a poll of workers and then start polling
	 * or just do inline handling.
	 *
	 * After some testing, we realize that we only have one incoming
	 * data FIFO, and its not even faster enough to keep up a single core.
	 * Thus it does not make any sense to have thpool anymore.
	 * Anyway, we kept this code, and do inline handling by default.
	 */
	if (0) {
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
