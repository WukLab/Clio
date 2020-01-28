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
LIST_HEAD(board_list);
pthread_spinlock_t proc_lock;
pthread_spinlock_t board_lock;

static int add_proc(char *proc_name, char *host_name,
		    unsigned int host_ip)
{
	struct proc_info *pi;

	pi = malloc(sizeof(*pi));
	if (!pi)
		return -ENOMEM;

	INIT_LIST_HEAD(&pi->list);
	strncpy(pi->proc_name, proc_name, PROC_NAME_LEN);
	strncpy(pi->host_name, host_name, PROC_NAME_LEN);
	pi->host_ip = host_ip;
	pi->flags = 0;
	memset(pi->vregion, 0, NR_VREGIONS * sizeof(struct vregion_info));

	pthread_spin_lock(&proc_lock);
	list_add(&pi->list, &proc_list);
	pthread_spin_unlock(&proc_lock);
}

static int add_board(char *board_name, unsigned int board_ip,
		     unsigned long mem_total)
{
	struct board_info *bi;

	bi = malloc(sizeof(*bi));
	if (!bi)
		return -ENOMEM;

	INIT_LIST_HEAD(&bi->list);
	strncpy(bi->name, board_name, BOARD_NAME_LEN);
	bi->board_ip = board_ip;
	bi->mem_total = mem_total;
	bi->mem_avail = mem_total;

	pthread_spin_lock(&board_lock);
	list_add(&bi->list, &board_list);
	pthread_spin_unlock(&board_lock);
}

static void dump_boards(void)
{
	struct board_info *bi;
	int i = 0;

	printf("Dumping Boards Info:\n");
	pthread_spin_lock(&board_lock);
	list_for_each_entry(bi, &board_list, list) {
		printf("[%2d] %16s %lu %lu\n",
			i, bi->name,
			bi->mem_total, bi->mem_avail);
		i++;
	}
	pthread_spin_unlock(&board_lock);
}

static void dump_procs(void)
{
	struct proc_info *pi;
	int i = 0;

	printf("Dumping Process Address Space Info:\n");
	pthread_spin_lock(&proc_lock);
	list_for_each_entry(pi, &proc_list, list) {
		printf("[%2d] %s %s\n",
			i, pi->proc_name, pi->host_name);
		i++;
	}
	pthread_spin_unlock(&proc_lock);
}

static int handle_alloc(void)
{

}

int main(int argc, char **argv)
{
	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&board_lock, PTHREAD_PROCESS_PRIVATE);

	add_board("board_0", 123, 4096);
	add_board("board_1", 786, 9192);
	dump_boards();

	add_proc("proc_0", "host_0", 123);
	add_proc("proc_1", "host_0", 123);
	dump_procs();
}
