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
#include <limits.h>
#include <stdio.h>

#include "core.h"

/*
 * This is a per-node global list,
 * it has information about all remote accessible boards.
 */
#define BOARD_HASH_ARRAY_BITS	(6)
DEFINE_HASHTABLE(board_list, BOARD_HASH_ARRAY_BITS);
pthread_rwlock_t board_lock ____cacheline_aligned;

__constructor
static void init_board_subsys(void)
{
	pthread_rwlock_init(&board_lock, NULL);
}

/*
 * We use ip and port to uniquely identify a legomem instance.
 * Thus the key is based on both of them.
 */
static __always_inline int
get_key(int ip, unsigned int port)
{
	return ip + port;
}

/*
 * Add a board to the system.
 * We will open a local session to connect with remote party's mgmt session.
 * The new session will be saved into the board_info structure.
 */
struct board_info *add_board(char *board_name, int board_ip, int udp_port)
{
	struct board_info *bi;
	int key;

	bi = malloc(sizeof(*bi));
	if (!bi)
		return NULL;

	init_board_info(bi);

	if (board_name)
		strncpy(bi->name, board_name, BOARD_NAME_LEN);
	bi->board_ip = board_ip;
	bi->udp_port = udp_port;

	key = get_key(bi->board_ip, bi->udp_port);
	pthread_rwlock_wrlock(&board_lock);
	hash_add(board_list, &bi->link, key);
	pthread_rwlock_unlock(&board_lock);

	return bi;
}

void remove_board(struct board_info *bi)
{
	int key;
	struct board_info *_bi;

	key = get_key(bi->board_ip, bi->udp_port);

	pthread_rwlock_wrlock(&board_lock);
	hash_for_each_possible(board_list, _bi, link, key) {
		if (_bi->board_ip == bi->board_ip &&
		    _bi->udp_port == bi->udp_port) {
			hash_del(&bi->link);
			pthread_rwlock_unlock(&board_lock);
			return;
		}
	}
	pthread_rwlock_unlock(&board_lock);
}

struct board_info *find_board(int ip, unsigned int port)
{
	struct board_info *bi;
	int key, bkt;

	key = get_key(ip, port);

	pthread_rwlock_rdlock(&board_lock);
	if (ip == ANY_BOARD || ip == ANY_NODE) {
		hash_for_each(board_list, bkt, bi, link) {
			if (special_board_info_type(bi->flags))
				continue;

			if (ip == ANY_BOARD) {
				if (!(bi->flags & BOARD_INFO_FLAGS_BOARD))
					continue;
			}

			pthread_rwlock_unlock(&board_lock);
			return bi;
		}
	} else {
		hash_for_each_possible(board_list, bi, link, key) {
			if (special_board_info_type(bi->flags))
				continue;

			if (bi->board_ip == ip &&
			    bi->udp_port == port) {
				pthread_rwlock_unlock(&board_lock);
				return bi;
			}
		}
	}
	pthread_rwlock_unlock(&board_lock);
	return NULL;
}

void dump_boards(void)
{
	struct board_info *bi;
	char ip_str[INET_ADDRSTRLEN];
	char ip_port_str[20];
	int bkt = 0;

	printf("bucket   board_name                     ip:port              type\n");
	printf("-------- ------------------------------ -------------------- ----------\n");

	pthread_rwlock_rdlock(&board_lock);
	hash_for_each(board_list, bkt, bi, link) {
		get_ip_str(bi->board_ip, ip_str);
		sprintf(ip_port_str, "%s:%d", ip_str, bi->udp_port);

		printf("%-8d %-30s %-20s %-10s\n",
			bkt, bi->name, ip_port_str,
			board_info_type_str(bi->flags));
	}
	pthread_rwlock_unlock(&board_lock);
}
