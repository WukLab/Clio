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

void test_buddy(void)
{
	unsigned long nr_pages;
	int i;
	struct timespec s, e;
	struct page *page;
	double diff_ns;

	nr_pages = (fpga_mem_end - fpga_mem_start) >> PAGE_SHIFT;
	nr_pages >>= 1;

	dprintf_INFO("NR_PAGES: %lu\n", nr_pages);

	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < nr_pages; i++) {
		page = alloc_page(0);
		if (!page) {
			dprintf_ERROR("Failed at %d th page alloc\n", i);
			exit(0);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	dprintf_INFO("nr_pages: %lu, allocated %d pages (order 0), avg latency: %lf ns\n",
		nr_pages, i, diff_ns / i);
}
