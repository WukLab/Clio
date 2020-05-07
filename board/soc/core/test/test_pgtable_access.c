/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */
#include <uapi/sched.h>
#include <uapi/thpool.h>
#include <fpga/pgtable.h>
#include <fpga/lego_mem_ctrl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>

#include "../core.h"
#include "../pgtable.h"

void test_pgtable_access(void)
{
	struct timespec s, e;
	struct timespec s1, e1;
	int i, nr_pte;
	struct lego_mem_pte *pte;
	double diff_ns, diff1_ns;

	nr_pte = FPGA_DRAM_PGTABLE_SIZE / sizeof(struct lego_mem_pte);

	/* FPGA */
	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < nr_pte; i++) {
		pte = (struct lego_mem_pte *)devmem_pgtable_base + i;

		/* minic alloc */
		pte->tag = 100;
		pte->allocated = 1;
		pte->valid = 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	/* SoC */
	clock_gettime(CLOCK_MONOTONIC, &s1);
	for (i = 0; i < nr_pte; i++) {
		pte = (struct lego_mem_pte *)soc_shadow_pgtable + i;

		/* minic alloc */
		pte->tag = 100;
		pte->allocated = 1;
		pte->valid = 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &e1);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	diff1_ns = (e1.tv_sec * NSEC_PER_SEC + e1.tv_nsec) -
		   (s1.tv_sec * NSEC_PER_SEC + s1.tv_nsec);

	printf("nr_ptes: %d\n"
	       "               Total (ns)        Avg (ns)\n"
	       "FPGA:          %10lf             %10lf\n"
	       "SoC Shadow:    %10lf             %10lf\n",
		nr_pte,
		diff_ns, diff_ns / i,
		diff1_ns, diff1_ns / i);


	/*
	 * The following tests
	 * want to find out if setting one more field
	 * will cause significant difference.
	 * it looks like it won't.
	 */

	/* FPGA */
	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < nr_pte; i++) {
		pte = (struct lego_mem_pte *)devmem_pgtable_base + i;

		pte->allocated = 0;
		pte->valid = 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	printf("nr_ptes: %d total: %lf avg: %lf\n",
		nr_pte,
		diff_ns, diff_ns / i);

	/* FPGA */
	clock_gettime(CLOCK_MONOTONIC, &s);
	for (i = 0; i < nr_pte; i++) {
		pte = (struct lego_mem_pte *)devmem_pgtable_base + i;

		pte->allocated = 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	printf("nr_ptes: %d total: %lf avg: %lf\n",
		nr_pte,
		diff_ns, diff_ns / i);
}
