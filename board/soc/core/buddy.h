/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#ifndef _LEGOMEM_SOC_BUDDY_H_
#define _LEGOMEM_SOC_BUDDY_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>

#include <uapi/list.h>
#include <uapi/log2.h>
#include <uapi/bitops.h>
#include <fpga/lego_mem_ctrl.h>
#include <fpga/fpga_memory_map.h>

/*
 * The managed physical memory range.
 * We do not allow any holes at this point.
 *
 * Due to the lego_mem_ctrl param32+param8 constraint,
 * we can support up to 2^40, or 1TB physical address range.
 *
 * This can be tuned.
 *
 * Current legomem memory map (include/fpga/fpga_memory_map.h):
 * 0-512M -> network cache
 * 512M - 1G -> pgtable
 * 1G - 2G -> data (buddy managed)
 *
 * There are physical DRAM address, not bus address.
 */
#define fpga_mem_start		FPGA_MEMORY_MAP_DATA_START
#define fpga_mem_end		FPGA_MEMORY_MAP_DATA_END

#define MAX_ORDER		(7)
#define MAX_ORDER_NR_PAGES	(1 << (MAX_ORDER - 1))

/*
 * Current smallest page size is 4M
 */
#define PAGE_SHIFT		(22)
#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((unsigned long)(x) << PAGE_SHIFT)
#define PHYS_PFN(x)	((unsigned long)((x) >> PAGE_SHIFT))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	ALIGN(addr, PAGE_SIZE)

/* test whether an address is aligned to PAGE_SIZE */
#define PAGE_ALIGNED(addr)	IS_ALIGNED((unsigned long)(addr), PAGE_SIZE)

#define max_pfn			PHYS_PFN(fpga_mem_end)

#define for_each_order(order) \
	for ((order) = 0; (order) < MAX_ORDER; (order)++)

struct free_area {
	struct list_head	free_list;
	unsigned long		nr_free;
};

struct page {
	unsigned long flags;
	unsigned long private;
	struct list_head list;
};

struct fpga_zone {
	struct free_area	free_area[MAX_ORDER];
	struct page		*page_map;
	pthread_spinlock_t	lock;

	/* Stats */
	unsigned long		nr_free;
};

#define page_private(page)		((page)->private)
#define set_page_private(page, v)	((page)->private = (v))

static inline unsigned int page_order(struct page *page)
{
	/* PageBuddy() must be checked by the caller */
	return page_private(page);
}

enum pageflags {
	PG_buddy,
};

/*
 * We don't have atomic version for aarch64 for now.
 */
#define TEST_PAGE_FLAG(uname, lname)				\
static inline int Page##uname(const struct page *page)		\
{								\
	return test_bit(PG_##lname, &page->flags);		\
}

#define __SET_PAGE_FLAG(uname, lname)				\
static inline void __SetPage##uname(struct page *page)		\
{								\
	__set_bit(PG_##lname, &page->flags);			\
}

#define __CLEAR_PAGE_FLAG(uname, lname)				\
static inline void __ClearPage##uname(struct page *page)	\
{								\
	__clear_bit(PG_##lname, &page->flags);			\
}

#define __TEST_SET_FLAG(uname, lname)				\
static inline int __TestSetPage##uname(struct page *page)	\
{								\
	return __test_and_set_bit(PG_##lname, &page->flags);	\
}

#define __TEST_CLEAR_FLAG(uname, lname)				\
static inline int __TestClearPage##uname(struct page *page)	\
{								\
	return __test_and_clear_bit(PG_##lname, &page->flags);	\
}

#define PAGE_FLAG(uname, lname)					\
	TEST_PAGE_FLAG(uname, lname)				\
	__SET_PAGE_FLAG(uname, lname)				\
	__CLEAR_PAGE_FLAG(uname, lname)				\
	__TEST_SET_FLAG(uname, lname)				\
	__TEST_CLEAR_FLAG(uname, lname)

PAGE_FLAG(Buddy, buddy)

extern unsigned long fpga_mem_start_soc_va;
extern struct fpga_zone *fpga_zone;

void dump_buddy(void);

/*
 * Runtime evaluation of get_order()
 */
static inline __attribute__((__const__))
int __get_order(unsigned long size)
{
	int order;

	size--;
	size >>= PAGE_SHIFT;
#if BITS_PER_LONG == 32
	order = fls(size);
#else
	order = fls64(size);
#endif
	return order;
}

/**
 * get_order - Determine the allocation order of a memory size
 * @size: The size for which to get the order
 *
 * Determine the allocation order of a particular sized block of memory.  This
 * is on a logarithmic scale, where:
 *
 *	0 -> 2^0 * PAGE_SIZE and below
 *	1 -> 2^1 * PAGE_SIZE to 2^0 * PAGE_SIZE + 1
 *	2 -> 2^2 * PAGE_SIZE to 2^1 * PAGE_SIZE + 1
 *	3 -> 2^3 * PAGE_SIZE to 2^2 * PAGE_SIZE + 1
 *	4 -> 2^4 * PAGE_SIZE to 2^3 * PAGE_SIZE + 1
 *	...
 *
 * The order returned is used to find the smallest allocation granule required
 * to hold an object of the specified size.
 *
 * The result is undefined if the size is 0.
 *
 * This function may be used to initialise variables with compile time
 * evaluations of constants.
 */
#define get_order(n)						\
(								\
	__builtin_constant_p((n)) ? (				\
		((n) == 0UL) ? BITS_PER_LONG - PAGE_SHIFT :	\
		(((n) < (1UL << PAGE_SHIFT)) ? 0 :		\
		 ilog2((n) - 1) - PAGE_SHIFT + 1)		\
	) :							\
	__get_order(n)						\
)

/*
 * Notes on these convertions:
 *
 * Despite that buddy allocator only manages [fpga_mem_start, fpga_mem_end],
 * we will prepare page_map and fpga_mem_start_soc_va for [0, fpga_mem_end].
 * Thus we don't need to do any PFN shifting.
 */

static __always_inline struct page *pfn_to_page(unsigned long pfn)
{
	BUG_ON(pfn >= max_pfn);
	return fpga_zone->page_map + pfn;
}

static __always_inline unsigned long page_to_pfn(struct page *page)
{
	unsigned long pfn;
	pfn = page - fpga_zone->page_map;
	BUG_ON(pfn >= max_pfn);
	return pfn;
}

static inline unsigned long pfn_to_soc_va(unsigned long pfn)
{
	return fpga_mem_start_soc_va + pfn * PAGE_SIZE;
}

static inline unsigned long page_to_soc_va(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);
	return pfn_to_soc_va(pfn);
}

#endif /* _LEGOMEM_SOC_BUDDY_H_ */
