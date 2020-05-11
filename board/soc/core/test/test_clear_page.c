/*
 * Eval buddy perf
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>
#include <uapi/compiler.h>

#include "../buddy.h"
#include "../core.h"

void test_clear_page(void)
{
	unsigned long nr_pages;
	int i;
	struct timespec s, e;
	double diff_ns;
	void *soc_va;
	unsigned long pfn;

	nr_pages = MAX_PFN - START_PFN;

	dprintf_INFO("NR_PAGES: %lu\n", nr_pages);

	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < nr_pages; i++) {
		/*
		 * It's okay to use unallocated pages
		 * we directly write into the page itself
		 * none buddy metadata is touched
		 */
		pfn = i + START_PFN;
		soc_va = (void *)pfn_to_soc_va(pfn);

#if 0
		dprintf_INFO("i %d pfn %lu soc_va %#lx\n", i, pfn, (u64)soc_va);
#endif

		clear_fpga_page(soc_va, PAGE_SIZE);
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	dprintf_INFO("nr_pages: %lu, page_size: %lx avg latency: %lf ns\n",
		nr_pages, PAGE_SIZE, diff_ns / i);
}
