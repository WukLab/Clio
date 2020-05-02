/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#ifndef _LEGOPGA_BOARD_SOC_CORE_H_
#define _LEGOPGA_BOARD_SOC_CORE_H_

#include <uapi/sched.h>
#include <uapi/thpool.h>
#include <fpga/zynq_regmap.h>
#include "external.h"

#if 0
# define LEGOMEM_DEBUG
#endif

#ifdef LEGOMEM_DEBUG
# define dprintf_DEBUG(fmt, ...) \
	printf("\033[34m[%s/%s()/%d]: " fmt "\033[0m", __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
# define dprintf_DEBUG(fmt, ...)  do { } while (0)
#endif

/* General info, always on */
#define dprintf_INFO(fmt, ...) \
	printf("\033[1;34m[%s/%s()/%d]: " fmt "\033[0m", __FILE__, __func__, __LINE__, __VA_ARGS__)

/* ERROR/WARNING info, always on */
#define dprintf_ERROR(fmt, ...) \
	printf("\033[1;31m[%s/%s()/%d]: " fmt "\033[0m", __FILE__, __func__, __LINE__, __VA_ARGS__)

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
struct proc_info *alloc_proc(unsigned int pid,
			     char *proc_name, unsigned int host_ip);
void free_proc(struct proc_info *pi);
struct proc_info *get_proc_by_pid(unsigned int pid);
void dump_proc(struct proc_info *pi);
void dump_procs(void);

/* Session */
int alloc_session_id(void);
void free_session_id(unsigned int session_id);

int pin_cpu(int cpu_id);
void getcpu(int *cpu, int *node);

#endif /* _LEGOPGA_BOARD_SOC_CORE_H_ */
