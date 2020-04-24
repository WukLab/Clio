/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Handlers running on SoC, also included by the host side board emulator.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

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

		vi = index_to_vregion(pi, vregion_idx);

		addr = alloc_va_vregion(pi, vi, len, vm_flags);
		if (unlikely(IS_ERR_VALUE(addr))) {
			resp->op.ret = -ENOMEM;
			resp->op.addr = 0xdeadbeef;
		} else {
			resp->op.ret = 0;
			resp->op.addr = addr;
		}
	} else {
		/* OP_REQ_FREE */
		unsigned long start, len;

		start = ops->addr;
		len = ops->len;
		resp->op.ret = free_va(pi, start, len);
	}

	dprintf_DEBUG("Op: %s. [%#lx - %#lx). pid=%u\n",
		is_alloc ? "alloc" : "free",
		is_alloc ? resp->op.addr : ops->addr,
		is_alloc ? resp->op.addr + ops->len - 1: ops->addr + ops->len -1,
		pid);

	put_proc_info(pi);
}
