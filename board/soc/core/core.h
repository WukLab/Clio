/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#ifndef _LEGOPGA_BOARD_SOC_CORE_H_
#define _LEGOPGA_BOARD_SOC_CORE_H_

#include <uapi/sched.h>

/* VM */
unsigned long alloc_va_vregion(struct proc_info *proc, struct vregion_info *vi,
			       unsigned long len, unsigned long vm_flags);
int free_va_vregion(struct proc_info *proc, struct vregion_info *vi,
	    unsigned long start, unsigned long len);
unsigned long alloc_va(struct proc_info *proc, unsigned long len, unsigned long vm_flags);
int free_va(struct proc_info *proc, unsigned long start, unsigned long len);
void test_vm(void);

/* SCHED */
int init_proc_subsystem(void);
struct proc_info *alloc_proc(char *proc_name, unsigned int host_ip);
void free_proc(struct proc_info *pi);
void dump_proc(struct proc_info *pi);
void dump_procs(void);
struct proc_info *get_proc_by_pid(unsigned int pid);

static inline void get_proc_info(struct proc_info *pi)
{
	/*
	 * fetch_add returns the old value,
	 * it must have >= 1 refcount before this func.
	 */
	if (unlikely(atomic_fetch_add(&pi->refcount, 1) < 1))
		BUG();
}

static inline void put_proc_info(struct proc_info *pi)
{
	/*
	 * fetch_sub returns the old value,
	 * thus we are the last user if it was 1.
	 */
	if (atomic_fetch_sub(&pi->refcount, 1) == 1)
		free_proc(pi);
}

#endif /* _LEGOPGA_BOARD_SOC_CORE_H_ */
