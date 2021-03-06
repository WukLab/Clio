/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * This file describes the DMA interface running on ZCU106 SoC.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>
#include <uapi/net_session.h>
#include <uapi/thpool.h>
#include <uapi/lego_mem.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <getopt.h>

#include "libaxidma.h"
#include "dma.h"
#include "core.h"

struct dma_info legomem_dma_info;

/*
 * The library is not SMP safe. Add lock for all sending path.
 * Both data and ctrl polling are done by one thread only,
 * thus no lock for them now.
 */
pthread_spinlock_t send_data_lock;
pthread_spinlock_t send_ctrl_lock;

/*
 * Callback for init_thpool_buffer
 * We will allocate DMA-able mmeory.
 */
int dma_thpool_alloc_cb(struct thpool_buffer *tb)
{
	axidma_dev_t dev;

	dev = legomem_dma_info.dev;

	tb->rx = axidma_malloc(dev, THPOOL_BUFFER_SIZE);
	if (!tb->rx)
		return -ENOMEM;

	tb->tx = axidma_malloc(dev, THPOOL_BUFFER_SIZE);
	if (!tb->tx)
		return -ENOMEM;

	tb->ctrl = axidma_malloc(dev, CTRL_BUFFER_SIZE);
	if (!tb->ctrl)
		return -ENOMEM;
	return 0;
}

/*
 * Init the DMA facility between ZCU106 SoC and FPGA.
 * We are currently using the libaxidma library.
 */
int init_dma(void)
{
	int ret;
	axidma_dev_t dev;
	const array_t *rx, *tx;

	pthread_spin_init(&send_data_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&send_ctrl_lock, PTHREAD_PROCESS_PRIVATE);

	dev = axidma_init();
	if (!dev) {
		printf("ERROR: fail to init AXI DMA device\n");
		return -ENODEV;
	}

	rx = axidma_get_dma_rx(dev);
	if (rx->len < 1) {
		printf("ERROR: fail to find rx channel\n");
		ret = -EIO;
		goto out_destroy;
	}

	tx = axidma_get_dma_tx(dev);
	if (tx->len < 1) {
		printf("ERROR: fail to find rx channel\n");
		ret = -EIO;
		goto out_destroy;
	}

	/* Save into the global varibale for future usage */
	legomem_dma_info.dev = dev;
	legomem_dma_info.rx = rx;
	legomem_dma_info.tx = tx;

	dprintf_INFO("AXIS DMA Library initialized... done! %d\n", 0);

	return 0;

out_destroy:
	axidma_destroy(dev);
	return ret;
}
