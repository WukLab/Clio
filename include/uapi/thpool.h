/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * Thread pool and buffer definitions and helpers.
 */

#ifndef _LEGOFPGA_UAPI_THPOOL_H_
#define _LEGOFPGA_UAPI_THPOOL_H_

#include <uapi/bitops.h>
#include <uapi/compiler.h>

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

	unsigned int		rx_size;
	char			rx[THPOOL_BUFFER_SIZE];

	unsigned int		tx_size;
	char			tx[THPOOL_BUFFER_SIZE];
};

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

#endif /* _LEGOFPGA_UAPI_THPOOL_H_ */
