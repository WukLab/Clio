/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */
#ifndef _PGTABLE_H_
#define _PGTABLE_H_

#include <uapi/sched.h>
#include <uapi/thpool.h>

void init_fpga_pgtable(void);

void setup_proc_fpga_pgtable(struct proc_info *pi);

void alloc_fpga_pte_range(struct proc_info *pi,
			  unsigned long start, unsigned long end,
			  unsigned long vm_flags, unsigned long page_size);
void free_fpga_pte_range(struct proc_info *pi,
			 unsigned long start, unsigned long end,
			 unsigned long page_size);

#endif /* _PGTABLE_H_ */
