/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

LIST_HEAD(proc_list);
pthread_spinlock_t proc_lock;

static struct proc_info *
add_proc(char *proc_name, char *host_name, unsigned int host_ip)
{
	struct proc_info *pi;
	int i;

	pi = malloc(sizeof(*pi));
	if (!pi)
		return NULL;

	INIT_LIST_HEAD(&pi->list);
	strncpy(pi->proc_name, proc_name, PROC_NAME_LEN);
	strncpy(pi->host_name, host_name, PROC_NAME_LEN);
	pi->host_ip = host_ip;
	pi->flags = 0;

	for (i = 0; i < NR_VREGIONS; i++) {
		struct vregion_info *v;

		v = pi->vregion + i;
		init_vregion(v);
	}

	pthread_spin_lock(&proc_lock);
	list_add(&pi->list, &proc_list);
	pthread_spin_unlock(&proc_lock);

	return pi;
}

static void dump_procs(void)
{
	struct proc_info *pi;
	int i = 0;

	printf("Dumping Process Address Space Info:\n");
	pthread_spin_lock(&proc_lock);
	list_for_each_entry (pi, &proc_list, list) {
		printf("[%2d] %s %s\n", i, pi->proc_name, pi->host_name);
		i++;
	}
	pthread_spin_unlock(&proc_lock);
}

static int handle_alloc(void)
{
}

unsigned long alloc_va(struct proc_info *proc, struct vregion_info *vi,
		       unsigned long len, unsigned long permission,
		       unsigned long flags);

int free_va(struct proc_info *proc,
	struct vregion_info *vi, unsigned long start, unsigned long len);

void test_va_alloc(struct proc_info *pi)
{
	unsigned long addr;
	struct vregion_info *vi;

	printf("From vregion 0\n");
	vi = pi->vregion + 0;
	addr = alloc_va(pi, vi, 0x1000, 0, 0);
	printf("%#lx\n", addr);
	addr = alloc_va(pi, vi, 0x1000, 0, VM_UNMAPPED_AREA_TOPDOWN);
	printf("%#lx\n", addr);

	printf("From vregion 1\n");

	vi = pi->vregion + 1;
	addr = alloc_va(pi, vi, 0x10000000, 0, VM_UNMAPPED_AREA_TOPDOWN);
	printf("1 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x10000000, 0, VM_UNMAPPED_AREA_TOPDOWN);
	printf("2 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x10000000, 0, 0);
	printf("3 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x10000000, 0, 0);
	printf("4 %#lx\n", addr);

	free_va(pi, vi, addr, 0x10000000);
	printf("free %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x2000, 0, 0);
	printf("5 %#lx\n", addr);

	addr = alloc_va(pi, vi, 0x2000, 0, 0);
	printf("6 %#lx\n", addr);
}

int main(int argc, char **argv)
{
	struct proc_info *pi;

	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);

	add_proc("proc_0", "host_0", 123);
	pi = add_proc("proc_1", "host_0", 123);
	dump_procs();

	test_va_alloc(pi);
}
