#ifndef _SOC_DMA_H_
#define _SOC_DMA_H_

#include "libaxidma.h"
#include <uapi/thpool.h>

#define DATA_SEND_CHANNEL 0
#define DATA_RECV_CHANNEL 1
#define CTRL_SEND_CHANNEL 2
#define CTRL_RECV_CHANNEL 3

/*
 * A convenient structure to carry information around
 * about the transfer
 */
struct dma_transfer {
	int ctrl_channel;      // The channel used to send the data
	int data_channel;      // The channel used to send the data
	void *input_buf;        // The buffer to hold the input data
};

struct dma_info {
	axidma_dev_t dev;
	const array_t *tx;
	const array_t *rx;
};

extern struct dma_info legomem_dma_info;

static inline int dma_recv_blocking(void *buf, size_t len)
{
	axidma_dev_t dev;

	dev = legomem_dma_info.dev;
	return axidma_oneway_transfer(dev, DATA_RECV_CHANNEL, buf, len, true);
}

static inline int dma_recv_nonblocking(void *buf, size_t len)
{
	axidma_dev_t dev;

	dev = legomem_dma_info.dev;
	return axidma_oneway_transfer(dev, DATA_RECV_CHANNEL, buf, len, false);
}

static inline int dma_send(void *buf, size_t len)
{
	axidma_dev_t dev;

	dev = legomem_dma_info.dev;
	return axidma_oneway_transfer(dev, DATA_SEND_CHANNEL, buf, len, true);
}

int init_dma(void);
int dma_thpool_alloc_cb(struct thpool_buffer *tb);

#endif /* _SOC_DMA_H_ */
