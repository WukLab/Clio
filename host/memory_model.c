/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/page.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "net/net.h"

void mc_wait_and_set_dependency(struct session_net *ses,
				unsigned long __remote addr, size_t total_size, int op)
{
	unsigned long start_index, nr_pages;

	start_index = addr >> PAGE_SHIFT;
	nr_pages = PAGE_ALIGN(total_size) >> PAGE_SHIFT;

	/*
	 * TODO
	 * add checking and wait logic.
	 * actually some sort of lock is needed. those bitmap
	 * operations are not atomic. a session can be used by multiple threads.
	 */

	switch (op) {
	case MEMORY_MODEL_OP_READ:
		bitmap_set(ses->outstanding_reads_map, start_index, nr_pages);
		break;
	case MEMORY_MODEL_OP_WRITE:
		bitmap_set(ses->outstanding_writes_map, start_index, nr_pages);
		break;
	default:
		BUG();
	}
}

void mc_clear_dependency(struct session_net *ses, unsigned long __remote addr,
			 size_t total_size, int op)
{
	unsigned long start_index, nr_pages;

	start_index = addr >> PAGE_SHIFT;
	nr_pages = PAGE_ALIGN(total_size) >> PAGE_SHIFT;

	switch (op) {
	case MEMORY_MODEL_OP_READ:
		bitmap_clear(ses->outstanding_reads_map, start_index, nr_pages);
		break;
	case MEMORY_MODEL_OP_WRITE:
		bitmap_clear(ses->outstanding_writes_map, start_index, nr_pages);
		break;
	default:
		BUG();
	}
}
