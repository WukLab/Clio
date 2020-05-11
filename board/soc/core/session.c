/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 * This file is copied from host/session.c with minor adjustment.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>
#include <uapi/thpool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "core.h"

static pthread_spinlock_t session_lock ____cacheline_aligned;
static struct session_net session_net_map[NR_MAX_SESSIONS_PER_NODE];
static DECLARE_BITMAP(session_id_map, NR_MAX_SESSIONS_PER_NODE);

__constructor
static void init_session_subsys(void)
{
	pthread_spin_init(&session_lock, PTHREAD_PROCESS_PRIVATE);
}

struct session_net *
find_net_session(unsigned int session_id)
{
	struct session_net *ses;

	if (unlikely(session_id >= NR_MAX_SESSIONS_PER_NODE))
		return NULL;

	ses = session_net_map + session_id;
	if (unlikely(!(ses->flags & SESSION_NET_FLAGS_ALLOCATED)))
		return NULL;

	return ses;
}

struct session_net *alloc_session(void)
{
	int bit;
	struct session_net *ses = NULL;

	/*
	 * note that find_next_zero_bit is not atomic,
	 * and we need to have lock here. Even though
	 * its possible to use test_and_set_bit without lock,
	 * use lock will do _no_ harm here.
	 *
	 * Skip session 0.
	 */
	pthread_spin_lock(&session_lock);
	bit = find_next_zero_bit(session_id_map, NR_MAX_SESSIONS_PER_NODE,
				 LEGOMEM_MGMT_SESSION_ID + 1);
	if (bit >= NR_MAX_SESSIONS_PER_NODE)
		goto unlock;

	__set_bit(bit, session_id_map);
	ses = session_net_map + bit;

	/* generic session init */
	init_session_net(ses);
	set_local_session_id(ses, bit);
	ses->flags |= SESSION_NET_FLAGS_ALLOCATED;

unlock:
	pthread_spin_unlock(&session_lock);
	return ses;
}

void free_session_by_id(unsigned int id)
{
	struct session_net *ses;

	if (id >= NR_MAX_SESSIONS_PER_NODE) {
		dprintf_ERROR("id too large: %d\n", id);
		return;
	}

	if (id == LEGOMEM_MGMT_SESSION_ID) {
		dprintf_ERROR("trying to free mgmt session %d\n",
			LEGOMEM_MGMT_SESSION_ID);
		return;
	}

	ses = session_net_map + id;

	pthread_spin_lock(&session_lock);
	if (!__test_and_clear_bit(id, session_id_map)) {
		dprintf_ERROR("WARN Session ID %d was free\n", id);
	}
	ses->flags = 0;
	pthread_spin_unlock(&session_lock);
}

void dump_net_sessions(void)
{
	int bkt;
	struct session_net *ses;
	char ip_str[INET_ADDRSTRLEN];
	char ip_port_str[20];

	printf("bucket   ses_local    ses_remote   ip:port_remote      \n"); 
	printf("-------- ------------ ------------ --------------------\n");

	pthread_spin_lock(&session_lock);
	for (bkt = 0; bkt < NR_MAX_SESSIONS_PER_NODE; bkt++) {
		ses = session_net_map + bkt;
		if (!(ses->flags & SESSION_NET_FLAGS_ALLOCATED))
			continue;

		get_ip_str(ses->board_ip, ip_str);
		sprintf(ip_port_str, "%s:%d", ip_str, ses->udp_port);

		printf("%-8d %-12u %-12u %-20s\n",
			bkt,
			get_local_session_id(ses),
			get_remote_session_id(ses),
			ip_port_str);
	}
	pthread_spin_unlock(&session_lock);
}
