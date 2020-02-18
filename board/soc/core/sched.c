/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * This file describes the process/task management part.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "core.h"

/*
 * We are using an array-based implementation for simplicity.
 * Alloc, free, search are fast.
 *
 * However, most API designs are generic and trying to be impl-oblivious,
 * this file can be easily extended to use list, hashtable etc in the future.
 */
#define NR_MAX_PROCS		(16)
static struct proc_info		proc_info_map[NR_MAX_PROCS];
static pthread_spinlock_t	alloc_lock;

#define for_each_proc_info(i, pi)		\
	for (i = 0, pi = proc_info_map;		\
	     i < NR_MAX_PROCS; i++, pi++)

void dump_proc(struct proc_info *pi)
{
	if (!pi)
		return;
	printf("PID: %d CMD: %s Host: %x\n",
		pi->pid, pi->proc_name, pi->host_ip);
}

void dump_procs(void)
{
	struct proc_info *pi;
	int i;

	printf("Dumping Process Address Space Info:\n");

	for_each_proc_info(i, pi)
		dump_proc(pi);
}

static void init_vregion(struct vregion_info *v)
{
	v->flags = VM_UNMAPPED_AREA_TOPDOWN;
	v->mmap = NULL;
	v->mm_rb = RB_ROOT;
	v->nr_vmas = 0;
	v->highest_vm_end = 0;
	pthread_spin_init(&v->lock, PTHREAD_PROCESS_PRIVATE);
}

static void init_proc_info(struct proc_info *pi)
{
	int j;
	struct vregion_info *v;

	pi->flags = 0;
	pi->nr_vmas = 0;
	INIT_LIST_HEAD(&pi->list);
	pthread_spin_init(&pi->lock, PTHREAD_PROCESS_PRIVATE);

	atomic_init(&pi->refcount, 1);

	for (j = 0; j < NR_VREGIONS; j++) {
		v = pi->vregion + j;
		init_vregion(v);
	}
}

static struct proc_info *__alloc_proc_info(void)
{
	int i;
	struct proc_info *pi;

	pthread_spin_lock(&alloc_lock);
	for_each_proc_info(i, pi) {
		if (pi->flags & PROC_INFO_FLAGS_ALLOCATED)
			continue;

		/* Find a free one */
		init_proc_info(pi);
		pi->flags = PROC_INFO_FLAGS_ALLOCATED;
		break;
	}
	pthread_spin_unlock(&alloc_lock);

	if (unlikely(i == NR_MAX_PROCS))
		return NULL;
	return pi;
}


/*
 * This is a public API to allocate a new process address space.
 * Allocate a new proc_info structure, initialize with the info supplied..
 */
struct proc_info *alloc_proc(char *proc_name, unsigned int host_ip)
{
	struct proc_info *pi;

	if (!proc_name)
		return NULL;

	pi = __alloc_proc_info();
	if (!pi)
		return NULL;

	strncpy(pi->proc_name, proc_name, PROC_NAME_LEN);
	pi->host_ip = host_ip;
	return pi;
}

void free_proc(struct proc_info *pi)
{
	if (!pi)
		return;

	if (atomic_load(&pi->refcount)) {
		printf("BUG: refcount is not zero.\n");
		return;
	}

	pthread_spin_lock(&alloc_lock);
	if (likely(pi->flags & PROC_INFO_FLAGS_ALLOCATED)) {
		pi->flags = 0;
		barrier();
	} else {
		printf("BUG: Invalid proc_info free\n");
		dump_proc(pi);
	}
	pthread_spin_unlock(&alloc_lock);
}

static inline bool is_pid_valid(unsigned int pid)
{
	if (likely(pid < NR_MAX_PROCS))
		return true;
	return false;
}

struct proc_info *get_proc_by_pid(unsigned int pid)
{
	struct proc_info *pi;

	if (unlikely(!is_pid_valid(pid)))
		return NULL;

	pi = proc_info_map + pid;
	get_proc_info(pi);
	return pi;
}

int init_proc_subsystem(void)
{
	int i;
	struct proc_info *p;

	pthread_spin_init(&alloc_lock, PTHREAD_PROCESS_PRIVATE);

	for_each_proc_info(i, p) {
		p->pid = i;
		init_proc_info(p);
	}
	return 0;
}
