/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * Thread pool and buffer definitions and helpers.
 * This is designed for the case where a single central polling thread, who will
 * distribute work to multiple workers. The design is efficient at the cost of
 * certain programming restrictions. For example, we use a ring buffer to
 * save the incoming requests, which, will be reused repeatly.
 * As a developer, you must be aware of this fact.
 *
 * Each worker thread is described by thpool_worker.
 * Each buffer pair (rx and tx) is described by thpool_buffer.
 *
 * All buffer pairs reside in a ring buffer, which is managed by the central
 * polling thread only, thus lockless, and a simple counter is sufficient.
 *
 * This is NOT some generic thpool implementation you are looking for.
 * This is tailored for our specific needs, just like the one we used in LegoOS.
 */

#ifndef _LEGOFPGA_UAPI_THPOOL_H_
#define _LEGOFPGA_UAPI_THPOOL_H_

#include <uapi/bitops.h>
#include <uapi/compiler.h>
#include <uapi/list.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define THPOOL_BUFFER_SIZE	(4096)

struct thpool_buffer;

struct tw_padding {
	char x[0];
} ____cacheline_aligned;
#define TW_PADDING(name)	struct tw_padding name

/*
 * This struct describes a single thread worker.
 */
struct thpool_worker {
	int			cpu;
	int			nr_queued;
	pthread_spinlock_t	lock;
	struct list_head	work_head;
	pthread_t		task;
	TW_PADDING(_pad1);

	/* stats */
	unsigned long		nr_handled;
} ____cacheline_aligned;

struct tb_padding {
	char x[0];
} __aligned(THPOOL_BUFFER_SIZE);
#define THPOOL_PADDING(name)	struct tb_padding name

/*
 * This struct describes a holder for both RX and TX buffers.
 */
struct thpool_buffer {
	unsigned int		flags;
	struct list_head	next;

	size_t			rx_size;
	void			*rx;

	unsigned int		tx_size;
	char			*tx;

	/*
	 * This field is only used by soc to
	 * send control message to setup_manager
	 */
	char			*ctrl;
} ____cacheline_aligned;

static inline void
set_tb_rx_size(struct thpool_buffer *tb, unsigned int rx_size)
{
	BUG_ON(rx_size > THPOOL_BUFFER_SIZE);
	tb->rx_size = rx_size;
}

static inline void
set_tb_tx_size(struct thpool_buffer *tb, unsigned int tx_size)
{
	BUG_ON(tx_size > THPOOL_BUFFER_SIZE);
	tb->tx_size = tx_size;
}

#define THPOOL_BUFFER_USED	0x1
#define THPOOL_BUFFER_NOREPLY	0x2
#define THPOOL_BUFFER_NOCONT	0x4

static inline bool ThpoolBufferUsed(struct thpool_buffer *tb)
{
	if (tb->flags & THPOOL_BUFFER_USED)
		return true;
	return false;
}

static inline bool ThpoolBufferNoreply(struct thpool_buffer *tb)
{
	if (tb->flags & THPOOL_BUFFER_NOREPLY)
		return true;
	return false;
}
static inline void SetThpoolBufferUsed(struct thpool_buffer *tb)
{
	tb->flags |= THPOOL_BUFFER_USED;
}

static inline void SetThpoolBufferNoreply(struct thpool_buffer *tb)
{
	tb->flags |= THPOOL_BUFFER_NOREPLY;
}

static inline void ClearThpoolBufferUsed(struct thpool_buffer *tb)
{
	tb->flags &= ~(unsigned long)THPOOL_BUFFER_USED;
}

static inline void ClearThpoolBufferNoreply(struct thpool_buffer *tb)
{
	tb->flags &= ~(unsigned long)THPOOL_BUFFER_NOREPLY;
}

/*
 * Both init_thpool and init_thpool_buffer are only called
 * once during startup. Caller needs tell us the number of workers
 * and the number of buffers to init.
 */
static inline int init_thpool(unsigned int NR_THPOOL_WORKERS,
			      struct thpool_worker **worker_map)
{
	int i;
	size_t buf_sz;
	struct thpool_worker *map;

	buf_sz = sizeof(struct thpool_worker) * NR_THPOOL_WORKERS;
	map = malloc(buf_sz);
	if (!map)
		return -ENOMEM;

	for (i = 0; i < NR_THPOOL_WORKERS; i++) {
		struct thpool_worker *tw;

		tw = map + i;
		tw->cpu = 0;
		tw->nr_queued = 0;
		INIT_LIST_HEAD(&tw->work_head);
		pthread_spin_init(&tw->lock, PTHREAD_PROCESS_PRIVATE);
	}
	*worker_map = map;
	return 0;
}

/*
 * The default alloc callback that uses malloc.
 * No special attachment or properties are assocaited with the buffer
 */
static inline int default_thpool_buffer_alloc_cb(struct thpool_buffer *tb)
{
	BUG_ON(!tb);

	tb->rx = malloc(THPOOL_BUFFER_SIZE);
	if (!tb->rx)
		return -ENOMEM;

	tb->tx = malloc(THPOOL_BUFFER_SIZE);
	if (!tb->tx)
		return -ENOMEM;
	return 0;
}

/*
 * @alloc_cb is a user-provided callback to alloc the RX and TX buffers
 * associated with this thpool_buffer struct. The reason behind this special
 * callback, instead of using a standard malloc, is that some callers may
 * wish to use buffers wish special properties, e.g., DMA-able memory,
 * or non-cachable memory.
 */
static inline int init_thpool_buffer(unsigned int NR_THPOOL_BUFFER,
				     struct thpool_buffer **buffer_map,
				     int (*alloc_cb)(struct thpool_buffer *tb))
{
	int i, ret;
	size_t buf_sz;
	struct thpool_buffer *map;

	if (!alloc_cb) {
		printf("%s(): Please provide an buffer allocation callback. "
		       "If you are not sure what is this, use the default one.\n",
		       __func__);
		return -EINVAL;
	}

	buf_sz = sizeof(struct thpool_buffer) * NR_THPOOL_BUFFER;
	map = malloc(buf_sz);
	if (!map)
		return -ENOMEM;

	for (i = 0; i < NR_THPOOL_BUFFER; i++) {
		struct thpool_buffer *tb;

		tb = map + i;
		tb->flags = 0;
		tb->rx_size = 0;
		tb->tx_size = 0;

		/*
		 * Use user-provided callback to init rx and tx buffers
		 * We are not freeing memory if something go wrong.. it's fine now,
		 * mostly the caller will just PANIC.
		 */
		tb->rx = NULL;
		tb->tx = NULL;
		tb->ctrl = NULL;

		ret = alloc_cb(tb);
		if (ret)
			return ret;

		/* Post-check */
		BUG_ON(!tb->rx || !tb->tx);
	}
	*buffer_map = map;
	return 0;
}

static inline int nr_queued_thpool_worker(struct thpool_worker *tw)
{
	return READ_ONCE(tw->nr_queued);
}

static inline void inc_queued_thpool_worker(struct thpool_worker *tw)
{
	tw->nr_queued++;
}

static inline void dec_queued_thpool_worker(struct thpool_worker *tw)
{
	tw->nr_queued--;
}

#endif /* _LEGOFPGA_UAPI_THPOOL_H_ */
