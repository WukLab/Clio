/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_KERNEL_H_
#define _LEGO_MEM_KERNEL_H_

#ifdef __SYNTHESIS__
# define PR(fmt, ...)	do { } while (0)
#else
# ifdef ENABLE_PR
#  define PR(fmt, ...)	printf("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
# else
#  define PR(fmt, ...)	do { } while (0)
# endif
#endif

// highlight display
#define dph(fmt, ...)                       \
	do {                                \
		printf("\033[0;32m");       \
		printf(fmt, ##__VA_ARGS__); \
		printf("\033[0m");          \
	} while (0)

#define round_up(x, y)		(((x-1) | y-1)+1)

#define HLS_BUG()	\
	do {		\
	} while (0)

#endif /* _LEGO_MEM_KERNEL_H_ */
