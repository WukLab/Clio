/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/list.h>
#include <uapi/sched.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"

/*
 * This is a per-node global legomem context list.
 * One context per process.
 */
static LIST_HEAD(context_list);
static pthread_spinlock_t context_lock;

int init_context_subsys(void)
{
	pthread_spin_init(&context_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
}

int add_legomem_context(struct legomem_context *p)
{
	pthread_spin_lock(&context_lock);
	list_add(&p->list, &context_list);
	pthread_spin_unlock(&context_lock);
	return 0;
}

int remove_legomem_context(struct legomem_context *p)
{
	BUG_ON(!p);

	pthread_spin_lock(&context_lock);
	list_del(&p->list);
	pthread_spin_unlock(&context_lock);

	return 0;
}

void dump_legomem_context(void)
{
	struct legomem_context *p;
	int i = 0;

	printf("-- Dumping LegoMem Contexts: --\n");
	pthread_spin_lock(&context_lock);
	list_for_each_entry(p, &context_list, list) {
		printf("[%2d] pid %d\n", i, p->pid);
		i++;
	}
	pthread_spin_unlock(&context_lock);
}

/*
 * Ways to identify a session
 *
 * Combo 1: board_ip + session_id
 * Combo 2: board_ip + tid
 */

/*
 * Add an open session to the per-context cached list.
 * Caller needs to make sure the ses is fully cooked,
 * i.e., thread id, board info are filled.
 */
int context_add_session(struct legomem_context *p, struct session_net *ses)
{
	int key;

	key = get_session_key(ses);

	pthread_spin_lock(&p->lock);
	hash_add(p->ht_sessions, &ses->ht_link_context, key);
	pthread_spin_unlock(&p->lock);

	return 0;
}

int context_remove_session(struct legomem_context *p, struct session_net *ses)
{
	struct session_net *_ses;
	int key;

	key = get_session_key(ses);

	pthread_spin_lock(&p->lock);
	hash_for_each_possible(p->ht_sessions, _ses, ht_link_context, key) {
		/*
		 * We do not need to check tid here,
		 * session_id+board_ip are sufficient.
		 */
		if (likely(_ses->session_id == ses->session_id &&
			   _ses->board_ip == ses->board_ip)) {
			hash_del(&ses->ht_link_context);
			pthread_spin_unlock(&p->lock);
			return 0;
		}
	}
	pthread_spin_unlock(&p->lock);

	return -1;
}

/*
 * Caller only knows @tid and @board_ip, and try to find out if there
 * was already an established session.
 */
struct session_net *context_find_session_by_ip(struct legomem_context *p,
					       pid_t tid,
					       unsigned int board_ip)
{
	int bkt;
	struct session_net *ses;

	pthread_spin_lock(&p->lock);
	hash_for_each(p->ht_sessions, bkt, ses, ht_link_context) {
		if (ses->board_ip == board_ip &&
		    ses->tid == tid) {
			pthread_spin_unlock(&p->lock);
			return ses;
		}
	}
	pthread_spin_unlock(&p->lock);
	return NULL;
}

/* Check comments on context_find_session_by_ip() */
struct session_net *context_find_session_by_board(struct legomem_context *p,
						  pid_t tid,
						  struct board_info *bi)
{
	return context_find_session_by_ip(p, tid, bi->board_ip);
}
