#ifndef _LEGOFPGA_BOARD_THPOOL_H_
#define _LEGOFPGA_BOARD_THPOOL_H_

#include <uapi/compiler.h>

#define THPOOL_BUFFER_SIZE	(4096)
#define NR_THPOOL_BUFFER	(64)

#define NR_THPOOL_WORKERS	(1)

struct thpool_buffer;

struct tw_padding {
	char x[0];
} ____cacheline_aligned;
#define TW_PADDING(name)	struct tw_padding name

struct thpool_worker {
	int			cpu;
	int			nr_queued;
	pthread_spinlock_t	lock;
	TW_PADDING(_pad1);

	struct thpool_buffer	*thpool_buffer_map;
	int			TB_HEAD;

	/* stats */
	unsigned long		nr_handled;
} ____cacheline_aligned;

struct tb_padding {
	char x[0];
} __aligned(THPOOL_BUFFER_SIZE);
#define THPOOL_PADDING(name)	struct tb_padding name

struct thpool_buffer {
	unsigned long		flags;
	unsigned int		buffer_size;
	char			buffer[THPOOL_BUFFER_SIZE];
};

static inline void
set_tb_buffer_size(struct thpool_buffer *tb, unsigned int buffer_size)
{
	BUG_ON(buffer_size > THPOOL_BUFFER_SIZE);
	tb->buffer_size = buffer_size;
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

#endif /* _LEGOFPGA_BOARD_THPOOL_H_ */
