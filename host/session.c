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
#define SESSION_HASH_ARRAY_BITS	(10)
static DEFINE_HASHTABLE(session_hash_array, SESSION_HASH_ARRAY_BITS);
static pthread_spinlock_t session_lock;

int init_net_session_subsys(void)
{
	pthread_spin_init(&session_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
}

int add_net_session(struct session_net *ses)
{
	int key;

	key = get_session_key(ses->board_ip, ses->session_id);

	pthread_spin_lock(&session_lock);
	hash_add(session_hash_array, &ses->ht_link_host, key);
	pthread_spin_unlock(&session_lock);

	return 0;
}

int remove_net_session(struct session_net *ses)
{
	struct session_net *_ses;
	int key;

	key = get_session_key(ses->board_ip, ses->session_id);

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
find_net_session(unsigned int board_ip, unsigned int session_id)
{
	struct session_net *ses;
	int key;

	key = get_session_key(board_ip, session_id);

	pthread_spin_lock(&session_lock);
	hash_for_each_possible(session_hash_array, ses, ht_link_host, key) {
		if (likely(ses->session_id == session_id &&
			   ses->board_ip == board_ip)) {
			pthread_spin_unlock(&session_lock);
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


	printf("-- Dump all network sessions: --\n");
	printf("  HashBucket | Remote_Board | Session_ID\n");
	pthread_spin_lock(&session_lock);
	hash_for_each(session_hash_array, bkt, ses, ht_link_host) {
		struct board_info *bi;
		
		bi = ses->board_info;
		printf("  %10d | %s   | %10u\n", bkt, bi->name, ses->session_id);
	}
	pthread_spin_unlock(&session_lock);
}
