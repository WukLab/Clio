/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/hashtable.h>
#include <uapi/bitops.h>
#include <uapi/sched.h>

static LIST_HEAD(board_list);
static pthread_spinlock_t(board_lock);

static DECLARE_BITMAP(pid_map, NR_MAX_PID);
static pthread_spinlock_t(pid_lock);

/*
 * For each board, the valid process address spaces are linked
 * within a hashtable. The key is a combination of PID and Host Node ID.
 * The PID is assigned by either Host or Monitor.
 */
#define PID_ARRAY_HASH_BITS	(5)
static DEFINE_HASHTABLE(proc_hash_array, PID_ARRAY_HASH_BITS);
static pthread_spinlock_t(proc_lock);

static inline int getKey(unsigned int pid, unsigned int node)
{
        return node * NR_MAX_PROCS_PER_NODE + pid;
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

	INIT_HLIST_NODE(&pi->link);
	pi->pid = 0;
	pi->node = 0;

	pthread_spin_init(&pi->lock, PTHREAD_PROCESS_PRIVATE);
	atomic_init(&pi->refcount, 1);

	pi->nr_vmas = 0;
	for (j = 0; j < NR_VREGIONS; j++) {
		v = pi->vregion + j;
		init_vregion(v);
	}
}

int alloc_pid(void)
{
	int bit;

	/*
	 * note that find_next_zero_bit is not atomic,
	 * and we need to have lock here. Even though
	 * its possible to use test_and_set_bit without lock,
	 * use lock will do harm here.
	 */
	pthread_spin_lock(&pid_lock);
	bit = find_next_zero_bit(pid_map, NR_MAX_PID, 1);
	if (bit >= NR_MAX_PID) {
		bit = -1;
		goto unlock;
	}
	__set_bit(bit, pid_map);

unlock:
	pthread_spin_unlock(&pid_lock);
	return bit;
}

void free_pid(unsigned int pid)
{
	BUG_ON(pid >= NR_MAX_PID);

	pthread_spin_lock(&pid_lock);
	if (!test_and_clear_bit(pid, pid_map))
		BUG();
	pthread_spin_unlock(&pid_lock);
}

static struct proc_info *
alloc_proc(unsigned int pid, unsigned int node,
	   char *proc_name, unsigned int host_ip)
{
	struct proc_info *new;
	struct proc_info *old;
	unsigned int key;

	new = malloc(sizeof(*new));
	if (!new)
		return NULL;
	init_proc_info(new);

	if (proc_name)
		strncpy(new->proc_name, proc_name, PROC_NAME_LEN);
	if (host_ip)
		new->host_ip = host_ip;

	key = getKey(pid, node);

	/* Insert into the hashtable */
	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, old, link, key) {
		if (unlikely(old->pid == pid && old->node == node)) {
			pthread_spin_unlock(&proc_lock);
			free(new);
			printf("alloc_proc: pid %u node %u exists\n",
				pid, node);
			return NULL;
		}
	}
	hash_add(proc_hash_array, &new->link, key);
	pthread_spin_unlock(&proc_lock);

	printf("alloc_proc: new proc pid %u node %u\n", pid, node);

	return new;
}

static struct board_info *
add_board(char *board_name, unsigned int board_ip, unsigned long mem_total)
{
	struct board_info *bi;

	bi = malloc(sizeof(*bi));
	if (!bi)
		return NULL;

	INIT_LIST_HEAD(&bi->list);
	strncpy(bi->name, board_name, BOARD_NAME_LEN);
	bi->board_ip = board_ip;
	bi->mem_total = mem_total;
	bi->mem_avail = mem_total;

	pthread_spin_lock(&board_lock);
	list_add(&bi->list, &board_list);
	pthread_spin_unlock(&board_lock);

	return bi;
}

static void dump_boards(void)
{
	struct board_info *bi;
	int i = 0;

	printf("Dumping Boards Info:\n");
	pthread_spin_lock(&board_lock);
	list_for_each_entry (bi, &board_list, list) {
		printf("[%2d] %16s %lu %lu\n", i, bi->name, bi->mem_total,
		       bi->mem_avail);
		i++;
	}
	pthread_spin_unlock(&board_lock);
}

static int handle_alloc(void)
{
	struct proc_info *proc;
	int pid, node;
	char *proc_name;
	int host_ip;

	pid = alloc_pid();
	if (pid < 0)
		return pid;

	node = 0;
	proc_name = NULL;
	host_ip = 0;

	proc = alloc_proc(pid, node, proc_name, host_ip);
	if (!proc) {
		free_pid(pid);
		return -ENOMEM;
	}

	return 0;
}

int main(int argc, char **argv)
{
	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&board_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&pid_lock, PTHREAD_PROCESS_PRIVATE);

	add_board("board_0", 123, 4096);
	add_board("board_1", 786, 9192);
	dump_boards();

	//alloc_proc("proc_0", "host_0", 123);
	//pi = alloc_proc("proc_1", "host_0", 123);
	//dump_procs();
}
