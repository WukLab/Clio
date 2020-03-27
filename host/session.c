/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/bitops.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"

/*
 * Note that session 0 is each node's local management session.
 * It accept all traffic from any boards, host, and monitors.
 *
 * To send a message to a remote node's management session,
 * the sender should embed session_id 0 in its outgoing msg.
 */

/*
 * This is a hashtable of all open sessions in this node.
 * Each session is uniquely identified with a combination of:
 * - Board IP
 * - Session ID (board local)
 *
 * This hashtable is designed to be large to avoid high collision rate,
 * thus avoid list walking during lookup.
 *
 * This hashtable by design is read-heavy: lookup happens in datapath,
 * modification only happens in control path via add and remove.
 *
 * Even though a spinlock is lightweight, it *maybe* better to use RCU or alike.
 * However, others have their own overhead. We table this for now.
 */
#define HASH_ARRAY_BITS	(10)
static DEFINE_HASHTABLE(session_hash_array, HASH_ARRAY_BITS);
static pthread_spinlock_t session_lock;

static DECLARE_BITMAP(session_id_map, NR_MAX_SESSIONS_PER_NODE);
static pthread_spinlock_t(session_id_lock);

int init_net_session_subsys(void)
{
	pthread_spin_init(&session_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&session_id_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
}

int add_net_session(struct session_net *ses)
{
	int key;

	/*
	 * For this global net session list, we only need
	 * session_id and board_id. We do not need to concern about tid.
	 * This applies to the full scope of this file.
	 */
	key = __get_session_key(ses->board_ip, ses->udp_port, ses->session_id, 0);

	pthread_spin_lock(&session_lock);
	hash_add(session_hash_array, &ses->ht_link_host, key);
	pthread_spin_unlock(&session_lock);

	return 0;
}

int remove_net_session(struct session_net *ses)
{
	struct session_net *_ses;
	int key;

	key = __get_session_key(ses->board_ip, ses->udp_port, ses->session_id, 0);

	/*
	 * Walk through the hash bucket and check for session_id and board_ip
	 * in case there were conflicts in the same bucket.
	 */
	pthread_spin_lock(&session_lock);
	hash_for_each_possible(session_hash_array, _ses, ht_link_host, key) {
		if (likely(_ses->session_id == ses->session_id &&
			   _ses->board_ip == ses->board_ip)) {
			hash_del(&ses->ht_link_host);
			pthread_spin_unlock(&session_lock);
			return 0;
		}
	}
	pthread_spin_unlock(&session_lock);
	return -1;
}

struct session_net *
find_net_session(unsigned int board_ip, unsigned int udp_port, unsigned int session_id)
{
	struct session_net *ses;
	int key;

	if (session_id == LEGOMEM_MGMT_SESSION_ID)
		return mgmt_session;

	key = __get_session_key(board_ip, udp_port, session_id, 0);

	pthread_spin_lock(&session_lock);
	hash_for_each_possible(session_hash_array, ses, ht_link_host, key) {
		if (likely(ses->session_id == session_id &&
			   ses->udp_port == udp_port &&
			   ses->board_ip == board_ip)) {
			pthread_spin_unlock(&session_lock);
			dprintf_DEBUG("%s %d\n", ses->board_info->name, ses->session_id);
			return ses;
		}
	}
	pthread_spin_unlock(&session_lock);

	return NULL; 
}

void dump_net_sessions(void)
{
	int bkt;
	struct session_net *ses;
	char ip_str[INET_ADDRSTRLEN];
	char ip_port_str[20];

	printf("  bucket    ses_local   ses_remote       ip:port_remote               remote_name\n"); 
	printf("-------- ------------ ------------ -------------------- -------------------------\n");

	pthread_spin_lock(&session_lock);
	hash_for_each(session_hash_array, bkt, ses, ht_link_host) {
		struct board_info *bi;

		bi = ses->board_info;
		get_ip_str(ses->board_ip, ip_str);
		sprintf(ip_port_str, "%s:%d", ip_str, bi->udp_port);

		printf("%8d %12u %12u %20s %25s\n",
			bkt,
			get_local_session_id(ses),
			get_remote_session_id(ses),
			ip_port_str,
			bi->name);
	}
	pthread_spin_unlock(&session_lock);
}

/*
 * Return positive session id if success.
 * Otherwise return -1.
 */
int alloc_session_id(void)
{
	int bit;

	/*
	 * note that find_next_zero_bit is not atomic,
	 * and we need to have lock here. Even though
	 * its possible to use test_and_set_bit without lock,
	 * use lock will do harm here.
	 *
	 * We skip session 0, since it is reserved as the mgmt session.
	 */
	pthread_spin_lock(&session_id_lock);
	bit = find_next_zero_bit(session_id_map, NR_MAX_SESSIONS_PER_NODE,
				 LEGOMEM_MGMT_SESSION_ID + 1);
	if (bit >= NR_MAX_SESSIONS_PER_NODE) {
		bit = -1;
		goto unlock;
	}
	__set_bit(bit, session_id_map);

unlock:
	pthread_spin_unlock(&session_id_lock);
	return bit;
}

void free_session_id(unsigned int session_id)
{
	BUG_ON(session_id >= NR_MAX_SESSIONS_PER_NODE);

	/* mgmt session can never be freed */
	BUG_ON(session_id == LEGOMEM_MGMT_SESSION_ID);

	pthread_spin_lock(&session_id_lock);
	if (!test_and_clear_bit(session_id, session_id_map)) {
		printf("%s(): WARN Session ID %d was free\n",
			__func__, session_id);
	}
	pthread_spin_unlock(&session_id_lock);
}
