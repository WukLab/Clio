/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

/*
 * This file handles the free page FIFOs.
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

#include "core.h"
#include "dma.h"
#include "buddy.h"

/*
 * FIXME
 * This is the on-chip FIFO depth.
 * This is dynamic, please make sure this is
 * smaller than the actual used FIFO depth.
 */
#define FREEPAGE_FIFO_DEPTH	(30)

struct fifo_info {
	/* page allocation order */
	unsigned int flags;
	unsigned int order;

	/* This FIFO depth */
	unsigned int depth;

	/* lego_mem_ctrl->addr */
	u8 addr;

	/* Stats */
	int nr_refills;
};

/*
 * This is the info array about all the available free page fifos we need to feed.
 * This array is indexed by order.
 */
struct fifo_info freepage_fifos[] = {
	/* 4MB, single page, order 0 */
	[0] = {
		.flags		= 1,
		.depth		= FREEPAGE_FIFO_DEPTH,
		.addr		= LEGOMEM_CTRL_ADDR_FREEPAGE_0,
	},
#if 0
	/* 16MB, order 2 */
	[2] = {
		.flags		= 1,
		.depth		= FREEPAGE_FIFO_DEPTH,
		.addr		= LEGOMEM_CTRL_ADDR_FREEPAGE_1,
	},

	/* 128MB, order 6 */
	[6] = {
		.flags		= 1,
		.depth		= FREEPAGE_FIFO_DEPTH,
		.addr		= LEGOMEM_CTRL_ADDR_FREEPAGE_2,
	},
#endif
};

#define for_each_fifo_info(i, fi, base) \
	for ((i) = 0, (fi) = (base); (i) < ARRAY_SIZE(base); (i)++, (fi)++)

static inline struct fifo_info *order_to_fifo_info(unsigned int order)
{
	return freepage_fifos + order;
}

static inline bool fifo_info_valid(struct fifo_info *fi)
{
	return fi->flags != 0;
}

static __always_inline unsigned int
get_order_from_ctrl(struct lego_mem_ctrl *ctrl)
{
	unsigned int size;
	unsigned int order;

	size = ctrl->param32;
	if (unlikely(!size)) {
		dprintf_ERROR("size is 0! ctrl: %p\n", ctrl);
		return MAX_ORDER;
	}

	order = get_order(size);
	return order;
}

static __always_inline
void prepare_send_ctrl(struct lego_mem_ctrl *ctrl, struct fifo_info *fi,
		       unsigned long new_pa)
{
	/*
	 * Coremem sees physical address as we do.
	 * We need to get the bus address.
	 */
	new_pa = new_pa + FPGA_MEMORY_MAP_MAPPING_BASE;

	/* 40bit for physical address */ 
	ctrl->param32 = new_pa & 0xFFFFFFFF;
	ctrl->param8 = (new_pa >> 32) & 0xFF;

	/* Send to coremem */
	ctrl->epid = LEGOMEM_CONT_MEM;
	ctrl->cmd = 0;

	/* Per-FIFO specific routing info */
	ctrl->addr = fi->addr;
}

/*
 * Handle the event when we get a msg from the freepage ACK FIFO.
 * We check its size, get is order, find the corresponding fifo channel,
 * then refill one single page.
 */
void handle_ctrl_freepage_ack(struct lego_mem_ctrl *rx,
			      struct lego_mem_ctrl *tx)
{
	unsigned int order;
	struct fifo_info *fi;
	unsigned long pfn, pa;

#if 0
	order = get_order_from_ctrl(rx);
#else
	order = 0;
#endif
	if (unlikely(order >= MAX_ORDER)) {
		dprintf_ERROR("invalid size %#x\n", rx->param32);
		return;
	}

	fi = order_to_fifo_info(order);
	pfn = alloc_pfn(fi->order);
	if (unlikely(!pfn)) {
		dprintf_ERROR("OOM %d\n", 0);
		return;
	}

	pa = PFN_PHYS(pfn);
	prepare_send_ctrl(tx, fi, pa);
	dma_ctrl_send(tx, sizeof(*tx));
	fi->nr_refills++;

	dprintf_DEBUG("order %d new pfn %lx nr_refill %d\n",
		order, pfn, fi->nr_refills);
}

/*
 * This function is called during startup
 * to pre-fill all the fifos.
 */
static void prefill_fifos(struct lego_mem_ctrl *ctrl)
{
	int i, j;
	struct fifo_info *fi;
	unsigned long new_pfn, new_pa;

	for_each_fifo_info(i, fi, freepage_fifos) {
		if (!fifo_info_valid(fi))
			continue;

		for (j = 0; j < fi->depth; j++) {
			new_pfn = alloc_pfn(fi->order);
			if (!new_pfn) {
				dprintf_ERROR("OOM! %d\n", 0);
				exit(1);
			}
			new_pa = PFN_PHYS(new_pfn);

			prepare_send_ctrl(ctrl, fi, new_pa);
			dma_ctrl_send(ctrl, sizeof(*ctrl));
		}

		dprintf_INFO("Prefill freepage FIFO: Order=%u, depth=%u\n",
			fi->order, fi->depth);
	}
}

/*
 * HACK: If you enable this, the produced binary
 * should only ran once.
 */
int init_freepage_fifo(void)
{
#if 0
	axidma_dev_t dev;
	struct fifo_info *fi;
	int i;
	struct lego_mem_ctrl *ctrl_send_buf;

	BUILD_BUG_ON(ARRAY_SIZE(freepage_fifos) > MAX_ORDER);

	for_each_fifo_info(i, fi, freepage_fifos) {
		if (!fifo_info_valid(fi))
			continue;
		fi->order = i;
	}

	dev = legomem_dma_info.dev;
	ctrl_send_buf = axidma_malloc(dev, CTRL_BUFFER_SIZE);

	/* Refill all the FIFOs */
	prefill_fifos(ctrl_send_buf);

	axidma_free(dev, ctrl_send_buf, CTRL_BUFFER_SIZE);
#endif
	return 0;
}
