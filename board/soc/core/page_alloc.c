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
#include <sys/sysinfo.h>

#include "core.h"

struct fpga_zone *fpga_zone;

static inline struct page *pfn_to_page(unsigned long pfn)
{
	return fpga_zone->page_map + pfn;
}

static inline unsigned long page_to_pfn(struct page *page)
{
	return page - fpga_zone->page_map;
}

/*
 * Locate the struct page for both the matching buddy in our
 * pair (buddy1) and the combined O(n+1) page they form (page).
 *
 * 1) Any buddy B1 will have an order O twin B2 which satisfies
 * the following equation:
 *     B2 = B1 ^ (1 << O)
 * For example, if the starting buddy (buddy2) is #8 its order
 * 1 buddy is #10:
 *     B2 = 8 ^ (1 << 1) = 8 ^ 2 = 10
 *
 * 2) Any buddy B will have an order O+1 parent P which
 * satisfies the following equation:
 *     P = B & ~(1 << O)
 */
static inline unsigned long
__find_buddy_index(unsigned long page_idx, unsigned int order)
{
	return page_idx ^ (1 << order);
}

static inline void set_page_order(struct page *page, unsigned int order)
{
	set_page_private(page, order);
	__SetPageBuddy(page);
}

static inline void rmv_page_order(struct page *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}

/*
 * This function checks whether a page is free && is the buddy
 * we can do coalesce a page and its buddy if
 * (a) the buddy is not in a hole &&
 * (b) the buddy is in the buddy system &&
 * (c) a page and its buddy have the same order &&
 *
 * For recording page's order, we use page_private(page).
 */
static inline int page_is_buddy(struct page *page, struct page *buddy, unsigned int order)
{
	if (PageBuddy(buddy) && page_order(buddy) == order)
		return 1;
	return 0;
}

static void
__free_one_page(struct fpga_zone *zone,
		struct page *page, unsigned long pfn, unsigned int order)
{
	unsigned long page_idx;
	unsigned long combined_idx;
	unsigned long buddy_idx;
	struct page *buddy;

	page_idx = pfn & ((1 << MAX_ORDER) - 1);

	while (order < MAX_ORDER - 1) {
		buddy_idx = __find_buddy_index(page_idx, order);
		buddy = page + (buddy_idx - page_idx);
		if (!page_is_buddy(page, buddy, order))
			goto done_merging;

		list_del(&buddy->list);
		zone->free_area[order].nr_free--;
		rmv_page_order(buddy);

		combined_idx = buddy_idx & page_idx;
		page = page + (combined_idx - page_idx);
		page_idx = combined_idx;
		order++;
	}

done_merging:
	set_page_order(page, order);

	list_add(&page->list, &zone->free_area[order].free_list);
	zone->free_area[order].nr_free++;
}

/*
 * Return 1 if page is at bad shape.
 * Otherwise return 0.
 */
static __always_inline int
free_pages_check(struct page *page)
{
	/*
	 * A used page was dequeued from buddy
	 * it should not have Buddy flag set.
	 */
	if (unlikely(PageBuddy(page)))
		return 1;
	return 0;
}

static __always_inline bool
free_pages_prepare(struct page *page, unsigned int order)
{
	int bad = 0;

	if (unlikely(order)) {
		int i;
		for (i = 1; i < (1 << order); i++) {
			if (unlikely(free_pages_check(page + i))) {
				bad++;
				continue;
			}
		}
	}

	bad += free_pages_check(page);
	if (bad)
		return true;
	return false;
}

static __always_inline void
free_one_page(struct fpga_zone *zone,
	      struct page *page, unsigned long pfn, unsigned int order)
{
	/*
	 * Sanity check.
	 * In case a bad page is being freed.
	 */
	if (unlikely(free_pages_prepare(page, order))) {
		dprintf_ERROR("page_pfn %lu (order: %u) is in buddy when freed.\n",
			pfn, order);
		return;
	}

	pthread_spin_lock(&zone->lock);
	__free_one_page(zone, page, pfn, order);
	pthread_spin_unlock(&zone->lock);
}

void free_pfn(unsigned long pfn, unsigned int order)
{
	struct fpga_zone *zone;
	struct page *page;

	zone = fpga_zone;
	page = pfn_to_page(pfn);

	free_one_page(zone, page, pfn, order);
}

void free_page(struct page *page, unsigned int order)
{
	struct fpga_zone *zone;
	unsigned long pfn;

	zone = fpga_zone;
	pfn = page_to_pfn(page);

	free_one_page(zone, page, pfn, order);
}

/* Recursively break a higher-order page into smaller ones */
static inline void
expand(struct fpga_zone *zone, struct page *page,
       int low, int high, struct free_area *area)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;

		/*
		 * Effectively break a big one into two smaller ones.
		 * Enqueue the second half (&page[size]) into lower-order area.
		 */
		list_add(&page[size].list, &area->free_list);
		area->nr_free++;
		set_page_order(&page[size], high);
	}
}

/*
 * Do the hard work of removing an element from the buddy allocator.
 * Go through the free lists and remove the smallest available page it.
 */
static __always_inline struct page *
__rmqueue(struct fpga_zone *zone, unsigned int order)
{
	unsigned int current_order;
	struct free_area *area;
	struct page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);

		page = list_first_entry_or_null(&area->free_list, struct page, list);
		if (!page)
			continue;

		list_del(&page->list);
		rmv_page_order(page);
		area->nr_free--;

		expand(zone, page, order, current_order, area);
		return page;
	}
	return NULL;
}

static __always_inline struct page *
rmqueue(struct fpga_zone *zone, unsigned int order)
{
	struct page *page;

	pthread_spin_lock(&zone->lock);
	page = __rmqueue(zone, order);
	pthread_spin_unlock(&zone->lock);

	return page;
}

struct page *alloc_page(unsigned int order)
{
	struct page *page;
	struct fpga_zone *zone;

	if (unlikely(order >= MAX_ORDER)) {
		dprintf_ERROR("MAX_ORDER: %d. Order: %d\n",
			MAX_ORDER, order);
		return NULL;
	}

	zone = fpga_zone;
	page = rmqueue(zone, order);
	if (page) {
		set_page_private(page, 0);
	}
	return page;
}

unsigned long alloc_pfn(unsigned int order)
{
	struct page *page;

	page = alloc_page(order);
	if (likely(page))
		return page_to_pfn(page);
	return 0;
}

/*
 * Free all pages into the pool.
 * This is only called once during boot.
 */
static void initial_free_pages(unsigned long start_pa, unsigned long end_pa)
{
	unsigned long start_pfn = PFN_UP(start_pa);
	unsigned long end_pfn = PFN_DOWN(end_pa);
	int order;

	BUG_ON(start_pfn > end_pfn);

	while (start_pfn < end_pfn) {
		order = min(MAX_ORDER - 1UL, __ffs(start_pfn));

		/* Adjust to avoid overflow */
		while (start_pfn + (1UL << order) > end_pfn)
			order--;

		free_page(pfn_to_page(start_pfn), order);

		start_pfn += (1UL << order);
	}
}

void dump_buddy(void)
{
	struct fpga_zone *zone;
	struct free_area *area;
	int order;
	unsigned long total_free_kb = 0;

	zone = fpga_zone;

	printf("Buddy Allocator\n");
	printf("\t-------- ---------- --------------------\n");
	printf("\torder    size (kb)  nr_free             \n");
	printf("\t-------- ---------- --------------------\n");
	for_each_order(order) {
		area = &zone->free_area[order];

		printf("\t%-8d %-10lu %-20lu\n",
			order,
			(PAGE_SIZE << order) >> 10,
			area->nr_free);

		total_free_kb += ((PAGE_SIZE << order) >> 10) * area->nr_free;
	}
	printf("\t-------- ---------- --------------------\n");
	printf("\tTotal Free: %lu kb, or around %lu mb\n",
		total_free_kb, total_free_kb >> 10);
}

void test_page_alloc(void)
{
#if 0
	struct page *p1, *p2, *p3, *p4;

	p1 = alloc_page(0);
	dump_buddy();

	p2 = alloc_page(0);
	dump_buddy();

	p3 = alloc_page(0);
	dump_buddy();

	p4 = alloc_page(0);
	dump_buddy();

	/* double free, should be error */
	free_page(p1, 0);
	free_page(p1, 0);
	dump_buddy();

	free_page(p2, 0);
	dump_buddy();

	free_page(p3, 0);
	dump_buddy();

	/* should be the same as original */
	free_page(p4, 0);
	dump_buddy();

	/* should be error */
	p1 = alloc_page(MAX_ORDER);
	dump_buddy();

	p1 = alloc_page(MAX_ORDER-1);
	dump_buddy();

	p1 = alloc_page(MAX_ORDER-2);
	dump_buddy();
#endif
}

/*
 * The managed physical memory range.
 * We do not allow any holes at this point.
 */
unsigned long fpga_mem_start = PAGE_SIZE;
unsigned long fpga_mem_end = 1024 * 1024 * 1024;

int init_page_alloc(void)
{
	struct fpga_zone *zone;
	int i, nr_pages;

	zone = malloc(sizeof(*zone));
	if (!zone)
		return -ENOMEM;

	pthread_spin_init(&zone->lock, PTHREAD_PROCESS_PRIVATE);
	zone->nr_free = 0;

	for (i = 0; i < MAX_ORDER; i++) {
		struct free_area *p;

		p = &zone->free_area[i];
		INIT_LIST_HEAD(&p->free_list);
		p->nr_free = 0;
	}
	fpga_zone = zone;

	/*
	 * Allocate page_map
	 * No matter what start is, we prepare all pages till the end.
	 */
	nr_pages = PHYS_PFN(fpga_mem_end);
	fpga_zone->page_map = malloc(nr_pages * sizeof(struct page));
	if (!fpga_zone->page_map) {
		printf("Fail to alloc page_map. nr_pages: %d\n", nr_pages);
		free(zone);
		return -ENOMEM;
	}

	initial_free_pages(fpga_mem_start, fpga_mem_end);

	dprintf_INFO("Buddy managing [%#018lx-%#018lx] Page Size %lu KB\n",
		fpga_mem_start, fpga_mem_end,
		PAGE_SIZE >> 10);
	dump_buddy();

	test_page_alloc();
	return 0;
}
