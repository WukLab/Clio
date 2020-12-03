/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

/*
 * This file is intended for VM conflict rate testing.
 * We will use external trace against our VM address space alloc/free algorighms.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/rbtree.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

#include "../core.h"

#define ALLOC_SIZE_4M	(1<<22)
#define ALLOC_SIZE_16M	(1<<24)
#define ALLOC_SIZE_64M	(1<<26)

#define OP_ALLOC (0)
#define OP_FREE  (1)
struct test_op {
	int op;
	unsigned long start, len;
};

void parse_trace(const char *fname, int nr_max_ops, struct test_op *ops)
{

}

/*
 * 1. alloc size
 * 2. nr retry
 * 3. policy?
 */
void test_vm_conflict(void)
{
	struct proc_info *pi;
	unsigned int pid;
	int i, nr_max_ops;
	struct test_op *ops, *op;

	/* Allocate a local proc for testing */
	pid = 100;
	pi = alloc_proc(pid, "proc_1", 0);
	if (!pi) {
		printf("fail to create the test pi\n");
		return;
	} 
	dump_procs();

	/* Prepare ops from trace file */
	nr_max_ops = 15000;
	ops = malloc(nr_max_ops * sizeof(*ops));
	if (!ops) {
		printf("OoM\n");
		return;
	}

	printf("Address space bits: %d. VREGION_SIZE bits: %d. NR_VREGIONS: %ld",
		VIRTUAL_ADDR_SHIFT, VREGION_SIZE_SHIFT, NR_VREGIONS);

#if 0
	const char fname[] = "trace.txt";
	parse_trace(fname, nr_max_ops, ops);
#endif

	for (i = 0; i < nr_max_ops; i++) {
		op = ops + i;
		op->op = OP_ALLOC;
		//op->len = VREGION_SIZE/2;
		op->len = PAGE_SIZE;
	}

	/* Do the work */
	unsigned long ret;
	int nr_retry;
	int sum_nr_retry = 0;
	/* for (i = 0; i < nr_max_ops; i++) { */
	/*         double util; */
        /*  */
	/*         op = ops + i; */
	/*         if (op->op == OP_ALLOC) { */
	/*                 ret = alloc_va(pi, op->len, 0, &nr_retry); */
	/*                 sum_nr_retry += nr_retry; */
        /*  */
	/*                 util  = ((double)nr_allocated_ptes / (double)FPGA_NUM_TOTAL_PTES) * 100; */
	/*                 printf("%d %#lx nr_retry=%d. PTE Util: %#ld / %#ld, %lf \%\n", i, ret, nr_retry, nr_allocated_ptes, FPGA_NUM_TOTAL_PTES, util); */
        /*  */
	/*                 if (ret == -ENOMEM) { */
	/*                         printf("%s: this alloc cannot be satisfied. i=%d\n", __func__, i); */
	/*                         break; */
	/*                 } */
	/*         } else if (op->op == OP_FREE) { */
	/*                 free_va(pi, op->start, op->len); */
	/*         } else */
	/*                 BUG(); */
	/* } */

	int agg_nr_retry = 0;
	long percent = 0;

	i = 0;
	while (1) {
		double util;

		op = ops;
		ret = alloc_va(pi, PAGE_SIZE * 100, 0, &nr_retry);
		sum_nr_retry += nr_retry;

		util  = ((double)nr_allocated_ptes / (double)FPGA_NUM_TOTAL_PTES) * 100;
		long t1 = (long)util;

		//printf("%ld %ld %d\n", t1, percent, (t1==percent));
		if (t1 == percent) {
			agg_nr_retry += nr_retry;
		} else {
			printf("%ld,%d\n", percent, agg_nr_retry);
			percent = (long)util;
			agg_nr_retry = nr_retry;
		}

		//printf("%d %#lx nr_retry=%d. PTE Util: %#ld / %#ld, %lf \%\n", i, ret, nr_retry, nr_allocated_ptes, FPGA_NUM_TOTAL_PTES, util);
		/* if (nr_retry) */
		/*         printf("%ld,%d\n", (long)util, nr_retry); */

		if (ret == -ENOMEM) {
			printf("%s: this alloc cannot be satisfied. i=%d\n", __func__, i);
			break;
		}
		i++;
	}

	printf("%s: total nr_retry=%d.\n", __func__, sum_nr_retry);
	dump_shadow_pgtable_util();
}
