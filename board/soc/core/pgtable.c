/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
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

#include "core.h"

/*
 * Called to free the physical pages for va range [start, end].
 */
void free_fpga_pte_range(struct proc_info *proc,
			 unsigned long start, unsigned long end)
{
}

/*
 * Update the PTEs that [start, start+len] mapped to.
 * We will mark the PTEs as Allocated, so the page fault handler
 * can know whether a certain VA was allocated by checking this bit
 * (similar to find_vma in linux ;/). But we do not allocate physical
 * memory at this point, that's passed along the free page list.
 */
void alloc_fpga_pte_range(struct proc_info *pi,
			  unsigned long start, unsigned long end, unsigned long vm_flags)
{

}
