/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 * This file describes the process/task management part.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>
#include <uapi/hashtable.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "core.h"

/*
 * For each board, the valid process address spaces are linked
 * within a hashtable. The key is a combination of PID and Host Node ID.
 * The PID is assigned by either Host or Monitor.
 */
#define PID_ARRAY_HASH_BITS	(5)
static DEFINE_HASHTABLE(proc_hash_array, PID_ARRAY_HASH_BITS);
static pthread_spinlock_t proc_lock;

static inline int getKey(unsigned int pid, unsigned int node)
{
        return node * NR_MAX_PROCS_PER_NODE + pid;
}

void dump_proc(struct proc_info *pi)
{
	if (!pi)
		return;
	printf("  PID: %d Host Node: %u COMM: %s Host: %x\n",
		pi->pid, pi->node, pi->proc_name, pi->host_ip);
}

void dump_procs(void)
{
	struct proc_info *p;
	int i;

	pthread_spin_lock(&proc_lock);
	hash_for_each(proc_hash_array, i, p, link) {
		dump_proc(p);
	}
	pthread_spin_unlock(&proc_lock);
}

/*
 * This is a public API to allocate a new process address space.
 * proc_name and host_ip are optional.
 * @pid is a globally unique id allocated by monitor.
 */
struct proc_info *alloc_proc(unsigned int pid, char *proc_name, unsigned int host_ip)
{
	struct proc_info *new;
	struct proc_info *old;
	unsigned key;

	new = malloc(sizeof(*new));
	if (!new)
		return NULL;
	init_proc_info(new);

	new->pid = pid;

	if (proc_name)
		strncpy(new->proc_name, proc_name, PROC_NAME_LEN);
	if (host_ip)
		new->host_ip = host_ip;

	key = pid;

	/* Insert into the hashtable */
	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, old, link, key) {
		if (unlikely(old->pid == pid)) {
			pthread_spin_unlock(&proc_lock);
			free(new);
			printf("alloc_proc: pid %u exists\n", pid);
			return NULL;
		}
	}
	hash_add(proc_hash_array, &new->link, key);
	pthread_spin_unlock(&proc_lock);

	printf("alloc_proc: new proc pid %u\n", pid);
	return new;
}

/*
 * Free the given pi and remove it from the hashtable.
 * The refcount must be 0 upon invocation.
 */
void free_proc(struct proc_info *pi)
{
	unsigned int pid, key;
	struct proc_info *tsk;

	if (!pi)
		return;

	if (atomic_load(&pi->refcount)) {
		printf("BUG: refcount is not zero. Use put_proc()\n");
		return;
	}

	pid = pi->pid;
	key = pid;

	/* Walk through all the possible buckets, check pid */
	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, tsk, link, key) {
		if (likely(tsk->pid == pid)) {
			hash_del(&tsk->link);
			pthread_spin_unlock(&proc_lock);
			free(tsk);
			return;
		}
	}
	pthread_spin_unlock(&proc_lock);
	printf("WARN: Fail to find tsk (pid %d)\n", pid);
}

/*
 * Find the pi structure by given pid.
 * The refcount is incremented by 1 if found.
 * The caller must call put_proc() afterwards.
 */
struct proc_info *get_proc_by_pid(unsigned int pid)
{
	struct proc_info *pi;
	unsigned int key;

	key = pid;

	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, pi, link, key) {
		if (likely(pi->pid == pid)) {
			get_proc_info(pi);
			pthread_spin_unlock(&proc_lock);
			return pi;
		}
	}
	pthread_spin_unlock(&proc_lock);
	return NULL;
}

__constructor
static int init_proc_subsystem(void)
{
	printf("%s here\n", __func__);
	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
}
