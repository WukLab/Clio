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
	nr_max_ops = 128;
	ops = malloc(nr_max_ops * sizeof(*ops));
	if (!ops) {
		printf("OoM\n");
		return;
	}

	const char fname[] = "trace.txt";
	parse_trace(fname, nr_max_ops, ops);

	/* Do the work */
	for (i = 0; i < nr_max_ops; i++) {
		op = ops + i;
		if (op->op == OP_ALLOC) {
			alloc_va(pi, op->len, 0);
		} else if (op->op == OP_FREE) {
			free_va(pi, op->start, op->len);
		} else
			BUG();
	}
}
