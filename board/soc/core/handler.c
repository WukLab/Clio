/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Handlers running on SoC, also included by the host side board emulator.
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

#include "core.h"

#ifdef CONFIG_ARCH_ARM64
#include "dma.h"
#endif

/*
 * Handle alloc/free requests from _host_.
 *
 * Note that
 * 1) This might be the first the host contacting with us,
 *    thus there might be no context created. We will create on if that's the case.
 * 2) Upon alloc, remote will tell us the vRegion index, which, was chosen by monitor.
 * 3) We only perform allocation withi one vRegion.
 *    But we could perform free spanning multiple vRegions.
 */
void board_soc_handle_alloc_free(struct thpool_buffer *tb, bool is_alloc)
{
	struct legomem_alloc_free_req *req;
	struct op_alloc_free *ops;
	struct legomem_alloc_free_resp *resp;
	struct lego_header *lego_header;
	struct proc_info *pi;
	pid_t pid;

	resp = (struct legomem_alloc_free_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
	resp->comm_headers.lego.opcode = is_alloc ? OP_REQ_ALLOC_RESP : OP_REQ_ALLOC_RESP;
	resp->comm_headers.lego.cont = MAKE_CONT(LEGOMEM_CONT_NET,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE,
						 LEGOMEM_CONT_NONE);

	req = (struct legomem_alloc_free_req *)tb->rx;
	ops = &req->op;
	lego_header = to_lego_header(req);

	pid = lego_header->pid;
	pi = get_proc_by_pid(pid);
	if (!pi) {
		/*
		 * This is correct and follows our current design flow:
		 * Neither monitor nor board will contact us when the context
		 * was first created. Monitor choose this board without checking
		 * whether this board has created the context or not.
		 *
		 * This makes the flow simple but we could not do authentication
		 * check at this point, we could only allocate the vRegion
		 * This kind of design is fine for a proof-of-concept system,
		 * but not okay for a production one.
		 *
		 * The simpliest fix is to let monitor contact the board when
		 * the monitor was chosing vRegion.
		 */
		dprintf_DEBUG("Create a new context for PID %d\n", pid);
		pi = alloc_proc(pid, NULL, 0);
		if (!pi) {
			resp->op.ret = -ENOMEM;
			return;
		}

		/* We will drop in the end */
		get_proc_by_pid(pid);
	}

	if (is_alloc) {
		/* OP_REQ_ALLOC */
		unsigned long addr, len, vregion_idx, vm_flags;
		struct vregion_info *vi;

		len = ops->len;
		vregion_idx = ops->vregion_idx;
		vm_flags = ops->vm_flags;

		if (vregion_idx == 0xFFFFFFFFDEADBEEF) {
			/*
			 * Special case crafted for multiverion on-chip modules
			 * It does not know vregion idx, we need to allocate here.
			 * Thus calling the __handle_ctrl_alloc wrapper.
			 */
			addr = __handle_ctrl_alloc(pi, len);
			resp->op.ret = 0;
			resp->op.addr = addr;
		} else {
			/*
			 * Normal case, requests came from hosts.
			 */
			vi = index_to_vregion(pi, vregion_idx);
			addr = alloc_va_vregion(pi, vi, len, vm_flags);
			if (unlikely(IS_ERR_VALUE(addr))) {
				resp->op.ret = -ENOMEM;
				resp->op.addr = 0xdeadbeef;
			} else {
				resp->op.ret = 0;
				resp->op.addr = addr;
			}
		}
	} else {
		/* OP_REQ_FREE */
		unsigned long start, len;

		start = ops->addr;
		len = ops->len;
		resp->op.ret = free_va(pi, start, len);
	}

	dprintf_DEBUG("Op: %s. size: %#lx [%#lx - %#lx] pid=%u vregion_idx=%lu\n",
		is_alloc ? "alloc" : "free",
		ops->len,
		is_alloc ? resp->op.addr : ops->addr,
		is_alloc ? resp->op.addr + ops->len - 1: ops->addr + ops->len -1,
		pid, ops->vregion_idx);

	put_proc_info(pi);
}

/*
 * Making it a macro because we want to know the calling function name.
 * Can we use builtin return ip without having something like kallsyms?
 */
#if 1
#define dump_migration_req(pid, op)						\
	do {									\
		char ip_src[INET_ADDRSTRLEN], ip_dst[INET_ADDRSTRLEN];		\
		get_ip_str((op)->src_board_ip, ip_src);				\
		get_ip_str((op)->dst_board_ip, ip_dst);				\
		dprintf_DEBUG("pid=%d vregion_idx=%u [%s:%d -> %s:%d]\n",	\
			pid, (op)->vregion_index,				\
			ip_src, (op)->src_udp_port,				\
			ip_dst, (op)->dst_udp_port);				\
	} while (0)
#else
#define dump_migration_req(pid, op)						\
	do {									\
	} while (0)
#endif

void *global_migration_req;

/*
 * This is not SMP safe.
 * But we are using INLINE handling now, its okay.
 */
static inline void *get_migration_req(void)
{
	BUG_ON(!global_migration_req);
	return global_migration_req;
}

void init_migration_setup(void)
{
	axidma_dev_t dev;
	void *p;

	dev = legomem_dma_info.dev;
	p = axidma_malloc(dev, THPOOL_BUFFER_SIZE);
	if (!p) {
		dprintf_ERROR("Fail to alloc the dma reqbuf for migration. %d\n", errno);
		exit(0);
	}
	global_migration_req = p;
}

static void do_read_migration(pid_t pid, int dst_board_ip, unsigned int vregion_index)
{
	struct __packed {
		struct lego_header lego_header;
		struct op_read_migration op;
	} *req;
	int i, nr_rounds;

	/* Get prepared DMA-able req */
	req = get_migration_req();

	/*
	 * Notify FPGA to start migration.
	 * There is packet size limit, so we do multiple rounds.
	 */
#define READ_MIGRATION_SIZE	(1<<10)
	req->lego_header.pid = pid;
	req->lego_header.opcode = OP_REQ_READ_MIGRATION;
	req->lego_header.cont = MAKE_CONT(LEGOMEM_CONT_MEM,
					    LEGOMEM_CONT_NET,
					    LEGOMEM_CONT_NONE,
					    LEGOMEM_CONT_NONE);
	/* Use session 0? */
	req->lego_header.src_sesid = 0;
	req->lego_header.dst_sesid = 0;
	req->lego_header.size = sizeof(*req);

	// XXX 192.168.1.22 1234
	req->lego_header.dest_ip = 0x011604d2;

	nr_rounds = VREGION_SIZE / READ_MIGRATION_SIZE;
	dprintf_DEBUG("nr_rounds: %d\n", nr_rounds);

	for (i = 0; i < nr_rounds; i++) {
		req->op.size = READ_MIGRATION_SIZE;
		req->op.va = vregion_index_to_va(vregion_index) + i * READ_MIGRATION_SIZE;
		dma_send(req, sizeof(*req));

		dprintf_DEBUG("migrationr req %5d len %u va %lx\n",
			i, req->op.size, req->op.va);
		sleep(10);
	}
	dprintf_DEBUG("Finished sending %d REQs to COREMEM...\n", i);
}

/*
 * Monitor asked us to migrate a certain vregion to a new node.
 * The new node has already been notified and waiting for us.
 *
 * Notes
 * 1. Monitor has made sure all hosts, boards will stop using
 *    this vRegion. Thus we do not to stop traffic at this board.
 * 2. The flow of this function is as follows:
 *    a) we will construct a legomem READ request to core-mem pipeline,
 *       which in turn will read data out and sent to network layer.
 *    a) we will free the local VMA resources
 */
void board_soc_handle_migration_send(struct thpool_buffer *tb)
{
	struct legomem_migration_req *req;
	struct legomem_migration_resp *resp;
	struct lego_header *lego_header;
	struct op_migration *op;
	struct proc_info *pi;
	struct vregion_info *vi;
	pid_t pid;

	/* Setup req and resp */
	req = (struct legomem_migration_req *)tb->rx;
	lego_header = to_lego_header(req);
	op = &req->op;

	resp = (struct legomem_migration_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	/* Find the task being migrated */
	pid = lego_header->pid;
	pi = get_proc_by_pid(pid);
	if (!pi) {
		dprintf_ERROR("pid %d not found\n", pid);
		resp->op.ret = -ESRCH;
		return;
	}
	dump_migration_req(pid, op);

	/* Do the dirty work */
	do_read_migration(pid, op->dst_board_ip, op->vregion_index);

	/* Reap all VMAs within this vRegion */
	vi = index_to_vregion(pi, op->vregion_index);
	free_va_vregion(pi, vi,
			vregion_index_to_va(op->vregion_index),
			vregion_index_to_va(op->vregion_index + 1));

	/* Done, success */
	resp->op.ret = 0;
	put_proc_info(pi);
}

/*
 * Monitor asked us to prepare for a upcoming migration.
 * We will create context, preapre vregion at this point.
 * After we reply, monitor will notify the original owner to send
 * the data to us. The data will just go through coremem pipeline.
 *
 * For a detailed flow, check monitor side migration manager.
 */
void board_soc_handle_migration_recv(struct thpool_buffer *tb)
{
	struct legomem_migration_req *req;
	struct legomem_migration_resp *resp;
	struct lego_header *lego_header;
	struct op_migration *op;
	struct proc_info *pi;
	struct vregion_info *vi;
	pid_t pid;
	unsigned long addr;

	req = (struct legomem_migration_req *)tb->rx;
	lego_header = to_lego_header(req);
	op = &req->op;

	resp = (struct legomem_migration_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	pid = lego_header->pid;
	pi = get_proc_by_pid(pid);
	if (!pi) {
		/*
		 * Similar to the comment on board_soc_handle_alloc_free.
		 * In this particular case, either host or board choose
		 * us as the destination board. But at this point, we may
		 * not have the Context. It's legit, just create a new one.
		 */
		dprintf_DEBUG("Create a new context for PID %d\n", pid);
		pi = alloc_proc(pid, NULL, 0);
		if (!pi) {
			resp->op.ret = -ENOMEM;
			return;
		}

		/* We will drop in the end */
		get_proc_by_pid(pid);
	}
	dump_migration_req(pid, op);

	/*
	 * XXX
	 * We do not have the VMA tree info for now.
	 * For now allocate the whole vRegion,
	 * make sure coremem pipeline is informed within.
	 */
	vi = index_to_vregion(pi, op->vregion_index);
	addr = alloc_va_vregion(pi, vi, VREGION_SIZE, 0);
	if (IS_ERR_VALUE(addr)) {
		dprintf_ERROR("Fail to prepare the vRegion %#lx\n", addr);
		resp->op.ret = -ENOMEM;
		goto put;
	}

	/* Done, success */
	resp->op.ret = 0;

put:
	put_proc_info(pi);
}

/*
 * Somehow the original owner cannot start the migration.
 * Thus we need to revert what we have done at the preparation stage.
 */
void board_soc_handle_migration_recv_cancel(struct thpool_buffer *tb)
{
	struct legomem_migration_req *req;
	struct legomem_migration_resp *resp;
	struct lego_header *lego_header;
	struct op_migration *op;
	struct vregion_info *vi;
	struct proc_info *pi;
	pid_t pid;

	req = (struct legomem_migration_req *)tb->rx;
	lego_header = to_lego_header(req);
	op = &req->op;

	resp = (struct legomem_migration_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	pid = lego_header->pid;
	pi = get_proc_by_pid(pid);
	if (!pi) {
		dprintf_ERROR("pid %d not found\n", pid);
		resp->op.ret = -ESRCH;
		return;
	}
	dump_migration_req(pid, op);
	
	/* Revert our actions at handle_migration_recv */
	vi = index_to_vregion(pi, op->vregion_index);
	free_va_vregion(pi, vi,
			vregion_index_to_va(op->vregion_index),
			vregion_index_to_va(op->vregion_index + 1) - 1);

	/* Done, success */
	resp->op.ret = 0;
	put_proc_info(pi);
}
