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
DEFINE_HASHTABLE(board_list, BOARD_HASH_ARRAY_BITS);
pthread_spinlock_t board_lock;

int init_board_subsys(void)
{
	pthread_spin_init(&board_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
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
struct board_info *add_board(char *board_name, unsigned long mem_total,
			     struct endpoint_info *remote_ei,
			     struct endpoint_info *local_ei,
			     bool is_local)
{
	struct board_info *bi;
	int key;
	struct session_net *ses;

	bi = malloc(sizeof(*bi));
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
	pthread_spin_lock(&board_lock);
	hash_add(board_list, &bi->link, key);
	pthread_spin_unlock(&board_lock);

	return bi;
}

void remove_board(struct board_info *bi)
{
	int key;
	struct board_info *_bi;

	key = get_key(bi->board_ip, bi->udp_port);

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

struct board_info *
find_board(unsigned int ip, unsigned int port)
{
	struct board_info *bi;
	int key, bkt;

	key = get_key(ip, port);

	pthread_spin_lock(&board_lock);
	if (ip == ANY_BOARD) {
		hash_for_each(board_list, bkt, bi, link) {
			if (special_board_info_type(bi->flags))
				continue;

			/* Return the first board encountered*/
			pthread_spin_unlock(&board_lock);
			return bi;
		}
	} else {
		hash_for_each_possible(board_list, bi, link, key) {
			if (special_board_info_type(bi->flags))
				continue;

			if (bi->board_ip == ip &&
			    bi->udp_port == port) {
				pthread_spin_unlock(&board_lock);
				return bi;
			}
		}
	}
	pthread_spin_unlock(&board_lock);
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
	pthread_spin_lock(&board_lock);
	hash_for_each(board_list, bkt, bi, link) {
		get_ip_str(bi->board_ip, ip_str);
		sprintf(ip_port_str, "%s:%d", ip_str, bi->udp_port);

		printf("%-8d %-30s %-20s %-10s\n",
			bkt, bi->name, ip_port_str,
			board_info_type_str(bi->flags));
	}
	pthread_spin_unlock(&board_lock);
}
