/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
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


void *global_tlbflush_req;

/*
 * This is not SMP safe.
 * But we are using INLINE handling now, its okay.
 */
static inline void *get_tlbflush_req(void)
{
	BUG_ON(!global_tlbflush_req);
	return global_tlbflush_req;
}

void init_tlbflush_setup(void)
{
	axidma_dev_t dev;
	void *p;

	dev = legomem_dma_info.dev;
	p = axidma_malloc(dev, THPOOL_BUFFER_SIZE);
	if (!p) {
		dprintf_ERROR("Fail to alloc the dma reqbuf for tlbflush. %d\n", errno);
		exit(0);
	}
	global_tlbflush_req = p;
}

/*
 * Send a TLB cache flush to coremem cached BRAM pgtable.
 * Simple semantic, just as you know it.
 */
void tlb_flush(struct proc_info *pi, unsigned long va, unsigned long size)
{
	struct __packed {
		struct lego_header lego_header;
		struct op_cache_flush op;
	} *req;

	req = get_tlbflush_req();

	req->lego_header.pid = pi->pid;
	req->lego_header.opcode = OP_REQ_CACHE_INVALID;
	req->lego_header.cont = MAKE_CONT(LEGOMEM_CONT_MEM, LEGOMEM_CONT_NONE,
					 LEGOMEM_CONT_NONE, LEGOMEM_CONT_NONE);
	req->lego_header.size = sizeof(*req);

	req->op.va = va;
	req->op.size = size;
	dma_send(req, sizeof(*req));

	dprintf_DEBUG("PID: %u TLB FLUSH VA @[%#lx - %#lx] size %#lx\n",
		pi->pid, va, va + size, size);
}
