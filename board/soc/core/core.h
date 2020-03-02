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

/*
 * SCHED APIs
 *
 * 1. alloc_proc: create a new proc and insert into hashtable
 * 2. free_proc: free the proc and delete from hashtable
 * 3. get_proc_by_pid: find the proc, and then increase refcount
 * 4. get_proc_info: increase refcount
 * 5. put_proc_info: decrese refcount
 *
 * Developers should use get_proc_by_pid, and put_proc_info in a pair
 * whenever you are using the proc_info. Never use free_proc directly.
 */
int init_proc_subsystem(void);
struct proc_info *alloc_proc(unsigned int pid, unsigned int node,
			     char *proc_name, unsigned int host_ip);
void free_proc(struct proc_info *pi);
struct proc_info *get_proc_by_pid(unsigned int pid, unsigned int node);
void dump_proc(struct proc_info *pi);
void dump_procs(void);

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
