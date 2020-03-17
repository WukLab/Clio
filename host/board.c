/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

/*
 * This is a per-node global list,
 * it has information about all remote accessible boards.
 */
#define HASH_ARRAY_BITS (5)
static DEFINE_HASHTABLE(board_list, HASH_ARRAY_BITS);
static pthread_spinlock_t board_lock;

int init_board_subsys(void)
{
	pthread_spin_init(&board_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
}

struct board_info *add_board(char *board_name, unsigned long mem_total,
			     struct endpoint_info *remote_ei,
			     struct endpoint_info *local_ei)
{
	struct board_info *bi;
	int key;

	bi = malloc(sizeof(*bi));
	if (!bi)
		return NULL;

	init_board_info(bi);

	memcpy(&bi->remote_ei, remote_ei, sizeof(struct endpoint_info));
	memcpy(&bi->local_ei, local_ei, sizeof(struct endpoint_info));

	if (board_name)
		strncpy(bi->name, board_name, BOARD_NAME_LEN);
	bi->board_ip = remote_ei->ip;
	bi->mem_total = mem_total;
	bi->mem_avail = mem_total;

	key = bi->board_ip;
	pthread_spin_lock(&board_lock);
	hash_add(board_list, &bi->link, key);
	pthread_spin_unlock(&board_lock);

	return bi;
}

void remove_board(struct board_info *bi)
{
	int key;
	struct board_info *_bi;

	key = bi->board_ip;

	pthread_spin_lock(&board_lock);
	hash_for_each_possible(board_list, _bi, link, key) {
		if (_bi->board_ip == bi->board_ip) {
			hash_del(&bi->link);
			pthread_spin_unlock(&board_lock);
			return;
		}
	}
	pthread_spin_unlock(&board_lock);
}

struct board_info *find_board_by_ip(unsigned int board_ip)
{
	struct board_info *bi;
	int key;

	key = board_ip;

	pthread_spin_lock(&board_lock);
	hash_for_each_possible(board_list, bi, link, key) {
		/* Return the first board in the list */
		if (board_ip == ANY_BOARD) {
			pthread_spin_unlock(&board_lock);
			return bi;
		}

		if (bi->board_ip == board_ip) {
			pthread_spin_unlock(&board_lock);
			return bi;
		}
	}
	pthread_spin_unlock(&board_lock);
	return NULL;
}

void dump_boards(void)
{
	struct board_info *bi;
	int i = 0;

	printf("-- Dumping Boards Info: --\n");
	printf("     Name  TotalMem  AvailMem\n");
	pthread_spin_lock(&board_lock);
	hash_for_each(board_list, i, bi, link) {
		printf("[%2d] %s %lu %lu\n", i, bi->name, bi->mem_total,
		       bi->mem_avail);
		i++;
	}
	pthread_spin_unlock(&board_lock);
}
