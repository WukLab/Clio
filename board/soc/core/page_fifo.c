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
#define FREEPAGE_FIFO_DEPTH	(2)

static struct lego_mem_ctrl *ctrl_recv_buf;
static struct lego_mem_ctrl *ctrl_send_buf;

struct fifo_info {
	/* page allocation order */
	unsigned int flags;
	unsigned int order;

	/* This FIFO depth */
	unsigned int depth;

	/* lego_mem_ctrl->addr */
	u8 addr;
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
		.addr		= 0,
	},

	/* 16MB, order 2 */
	[2] = {
		.flags		= 1,
		.depth		= FREEPAGE_FIFO_DEPTH,
		.addr		= 1,
	},

	/* 128MB, order 6 */
	[6] = {
		.flags		= 1,
		.depth		= FREEPAGE_FIFO_DEPTH,
		.addr		= 2,
	},
};

#define for_each_fifo_info(i, fi, base) \
	for ((i) = 0, (fi) = (base); (i) < ARRAY_SIZE(base); (i)++, (fi)++)

static inline struct fifo_info *order_to_fifo_into(unsigned int order)
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
#define FPGA_STORAGE_BASE_ADDRESS (540000000UL)
	new_pa = new_pa + FPGA_STORAGE_BASE_ADDRESS;

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

	order = get_order_from_ctrl(rx);
	if (unlikely(order >= MAX_ORDER)) {
		dprintf_ERROR("invalid size %#x\n", rx->param32);
		return;
	}
	fi = order_to_fifo_into(order);

	pfn = alloc_pfn(fi->order);
	if (unlikely(!pfn)) {
		dprintf_ERROR("OOM %d\n", 0);
		return;
	}

	pa = PFN_PHYS(pfn);
	prepare_send_ctrl(tx, fi, pa);
	dma_ctrl_send(tx, sizeof(*tx));
}

static void *ctrl_poll_func(void *_unused)
{
	int cpu, node;

	legomem_getcpu(&cpu, &node);
	dprintf_INFO("CTRL AXIS DMA Polling Thread running on CPU %d\n", cpu);

	while (1) {
		while (dma_ctrl_recv_blocking(ctrl_recv_buf, CTRL_BUFFER_SIZE) < 0)
			;

		dprintf_DEBUG("Get one. addr %#lx\n", (unsigned long)(ctrl_recv_buf->addr));
		handle_ctrl_freepage_ack(ctrl_recv_buf, ctrl_send_buf);
	}
	return NULL;
}

/*
 * This function is called during startup
 * to pre-fill all the fifos.
 */
static void prefill_fifos(void)
{
	int i, j;
	struct fifo_info *fi;
	unsigned long new_pfn, new_pa;
	struct lego_mem_ctrl *ctrl;

	ctrl = ctrl_send_buf;

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
 * 1. allocate buffers
 * 2. prefill all the free page fifos
 * 3. create a background polling thread
 */
int init_freepage_fifo(void)
{
	pthread_t t;
	axidma_dev_t dev;
	int i, ret;
	struct fifo_info *fi;

	BUILD_BUG_ON(ARRAY_SIZE(freepage_fifos) > MAX_ORDER);

	for_each_fifo_info(i, fi, freepage_fifos) {
		if (!fifo_info_valid(fi))
			continue;

		fi->order = i;
	}

	/* Allocate DMA-able send/recve buffers */
	dev = legomem_dma_info.dev;
	ctrl_send_buf = axidma_malloc(dev, CTRL_BUFFER_SIZE);
	ctrl_recv_buf = axidma_malloc(dev, CTRL_BUFFER_SIZE);

	prefill_fifos();

	dprintf_INFO("CTRL send buf: %#lx recv buf: %#lx\n",
		(unsigned long)ctrl_send_buf, (unsigned long)ctrl_recv_buf);

	ret = pthread_create(&t, NULL, ctrl_poll_func, NULL);
	if (ret) {
		dprintf_ERROR("Fail to launch the freepage list poll func %d\n", ret);
		exit(1);
	}
	return 0;
}
