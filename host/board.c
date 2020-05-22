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

#define NR_MAX_BOARDS	128

/*
 * This is legacy code. We keep it here because
 * some code is still using this hashtable list.
 */
DEFINE_HASHTABLE(board_list, BOARD_HASH_ARRAY_BITS);
pthread_rwlock_t board_lock ____cacheline_aligned;

static struct board_info board_info_map[NR_MAX_BOARDS];
static DECLARE_BITMAP(board_id_map, NR_MAX_BOARDS);

int nr_max_board_id ____cacheline_aligned;

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

static struct board_info *alloc_board_info(void)
{
	int bit;
	struct board_info *bi = NULL;

	pthread_rwlock_wrlock(&board_lock);
	bit = find_next_zero_bit(board_id_map, NR_MAX_BOARDS, 0);
	if (bit >= NR_MAX_BOARDS)
		goto unlock;

	/* Claim the ID */
	__set_bit(bit, board_id_map);
	nr_max_board_id = bit;

	bi = board_info_map + bit;
	memset(bi, 0, sizeof(*bi));
	bi->board_id = bit;
	bi->flags = BOARD_INFO_FLAGS_ALLOCATED;

unlock:
	pthread_rwlock_unlock(&board_lock);
	return bi;
}

/*
 * Add a board to the system.
 * We will open a local session to connect with remote party's mgmt session.
 * The new session will be saved into the board_info structure.
 */
struct board_info *add_board(char *board_name, unsigned long mem_total,
			     struct endpoint_info *remote_ei,
			     struct endpoint_info *local_ei,
			     bool is_local)
{
	struct board_info *bi;
	int key;
	struct session_net *ses;

	bi = alloc_board_info();
	if (!bi)
		return NULL;
	init_board_info(bi);

	memcpy(&bi->remote_ei, remote_ei, sizeof(struct endpoint_info));
	memcpy(&bi->local_ei, local_ei, sizeof(struct endpoint_info));

	if (board_name)
		strncpy(bi->name, board_name, BOARD_NAME_LEN);

	/*
	 * Save the info
	 * Note board_ip is in host order
	 * Each host could probably have multiple legomem instances
	 * thus we save udp_port to distinguish them
	 */
	bi->board_ip = remote_ei->ip;
	bi->udp_port = remote_ei->udp_port;
	bi->mem_total = mem_total;
	bi->mem_avail = mem_total;

	/*
	 * We will create a local session for each bi's mgmt session.
	 * Since some board_info are used to represent special local
	 * data structures such as localhost, local_mgmt etc, they
	 * will fall into the "is_local" catagory.
	 */
	if (is_local)
		ses = legomem_open_session_local_mgmt(bi);
	else
		ses = legomem_open_session_remote_mgmt(bi);
	set_board_mgmt_session(bi, ses);

	/*
	 * Now everything is ready, add it to the list
	 * and it will be visible across this node.
	 */
	key = get_key(bi->board_ip, bi->udp_port);

	pthread_rwlock_wrlock(&board_lock);
	hash_add(board_list, &bi->link, key);
	pthread_rwlock_unlock(&board_lock);

	return bi;
}

void remove_board(struct board_info *bi)
{
	int key, id;
	struct board_info *_bi;

	key = get_key(bi->board_ip, bi->udp_port);

	pthread_rwlock_wrlock(&board_lock);
	hash_for_each_possible(board_list, _bi, link, key) {
		if (_bi->board_ip == bi->board_ip) {
			id = bi - board_info_map;
			if (!__test_and_clear_bit(id, board_id_map))
				BUG();
			hash_del(&bi->link);

			if (id == nr_max_board_id)
				nr_max_board_id--;
			break;
		}
	}
	pthread_rwlock_unlock(&board_lock);
}

struct board_info *find_board_by_id(unsigned int id)
{
	struct board_info *bi;

	if (unlikely(id > NR_MAX_BOARDS)) {
		dprintf_ERROR("board id %d not valid\n", id);
		dump_boards();
		return NULL;
	}

	bi = board_info_map + id;
	if (!(bi->flags & BOARD_INFO_FLAGS_ALLOCATED)) {
		dprintf_ERROR("board id %d not allocated\n", id);
		dump_boards();
		return NULL;
	}
	return bi;
}

/*
 * This is DEPRECATED API.
 * Use find_board_by_id instead.
 */
struct board_info *
find_board(unsigned int ip, unsigned int port)
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

	dprintf_CRIT("Dump All Boards (nr_max_board_id %d)\n", nr_max_board_id);
	printf("  bucket   board_id   board_name                     ip:port              type\n");
	printf("  -------- ---------- ------------------------------ -------------------- ----------\n");

	pthread_rwlock_rdlock(&board_lock);
	hash_for_each(board_list, bkt, bi, link) {
		get_ip_str(bi->board_ip, ip_str);
		sprintf(ip_port_str, "%s:%d", ip_str, bi->udp_port);

		printf("  %-8d %-10d %-30s %-20s %-10s\n",
			bkt, bi->board_id, bi->name, ip_port_str,
			board_info_type_str(bi->flags));
	}
	pthread_rwlock_unlock(&board_lock);
	printf("\n");
}
