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
 * This is a per-node global legomem context hashtable.
 * One context per process.
 */
#define CONTEXT_HASH_ARRAY_BITS	(4)
static DEFINE_HASHTABLE(context_list, CONTEXT_HASH_ARRAY_BITS);
static pthread_spinlock_t context_lock;

int init_context_subsys(void)
{
	pthread_spin_init(&context_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
}

int add_legomem_context(struct legomem_context *p)
{
	unsigned int key;

	key = p->pid;

	pthread_spin_lock(&context_lock);
	hash_add(context_list, &p->link, key);
	pthread_spin_unlock(&context_lock);

	return 0;
}

int remove_legomem_context(struct legomem_context *p)
{
	unsigned int key;
	struct legomem_context *_p;

	key = p->pid;

	pthread_spin_lock(&context_lock);
	hash_for_each_possible(context_list, _p, link, key) {
		if (_p->pid == p->pid) {
			hash_del(&p->link);
			pthread_spin_unlock(&context_lock);
			return 0;
		}
	}
	pthread_spin_unlock(&context_lock);
	return -ESRCH;
}

struct legomem_context *find_legomem_context(unsigned int pid)
{
	unsigned int key;
	struct legomem_context *p;

	key = pid;
	pthread_spin_lock(&context_lock);
	hash_for_each_possible(context_list, p, link, key) {
		if (p->pid == pid) {
			pthread_spin_unlock(&context_lock);
			return p;
		}
	}
	pthread_spin_unlock(&context_lock);
	return NULL;
}

void dump_legomem_context(void)
{
	struct legomem_context *p;
	int bkt = 0;

	printf("  bucket      pid\n");
	printf("-------- --------\n");
	pthread_spin_lock(&context_lock);
	hash_for_each(context_list, bkt, p, link) {
		printf("%8d %8d\n", bkt, p->pid);
	}
	pthread_spin_unlock(&context_lock);
}

/*
 * Note about context_add_session/search/remove
 *
 * This per-context hashtable is used to cache per-context open-sessions.
 * It's specifically used to check if a certain thread has already open a
 * network session with a certain board. Thus the key for this hashtable
 * is tid (local thread PID), and boar ID (ip+udp_port).
 */

static inline int __get_key(pid_t tid, int board_ip, unsigned int port)
{
	return tid + board_ip + port;
}

static inline int get_key(struct session_net *ses)
{
	return __get_key(ses->tid, ses->board_ip, ses->udp_port);
}

/*
 * This is invoked when a new session is open.
 */
int context_add_session(struct legomem_context *p, struct session_net *ses)
{
	int key;

	key = get_key(ses);

	pthread_spin_lock(&p->lock);
	hash_add(p->ht_sessions, &ses->ht_link_context, key);
	pthread_spin_unlock(&p->lock);

	return 0;
}

int context_remove_session(struct legomem_context *p, struct session_net *ses)
{
	struct session_net *_ses;
	int key;

	key = get_key(ses);

	pthread_spin_lock(&p->lock);
	hash_for_each_possible(p->ht_sessions, _ses, ht_link_context, key) {
		if (likely(_ses->tid == ses->tid &&
			   _ses->udp_port == ses->udp_port &&
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
 * Caller only knows @tid and @board_ip+@udp_port,
 * and try to find out if there was already an established session between them.
 *
 * This is called during legomem_alloc.
 */
struct session_net *
context_find_session(struct legomem_context *p, pid_t tid,
		     int board_ip, unsigned udp_port)
{
	struct session_net *_ses;
	int key;

	key = __get_key(tid, board_ip, udp_port);

	pthread_spin_lock(&p->lock);
	hash_for_each_possible(p->ht_sessions, _ses, ht_link_context, key) {
		if (likely(_ses->tid == tid &&
			   _ses->udp_port == udp_port &&
			   _ses->board_ip == board_ip)) {
			pthread_spin_unlock(&p->lock);
			return _ses;
		}
	}
	pthread_spin_unlock(&p->lock);
	return NULL;
}

struct session_net *context_find_session_by_board(struct legomem_context *p,
						  pid_t tid,
						  struct board_info *bi)
{
	return context_find_session(p, tid, bi->board_ip, bi->udp_port);
}
