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

static pthread_spinlock_t(session_lock);
static struct session_net session_net_map[NR_MAX_SESSIONS_PER_NODE];
static DECLARE_BITMAP(session_id_map, NR_MAX_SESSIONS_PER_NODE);

int init_net_session_subsys(void)
{
	pthread_spin_init(&session_lock, PTHREAD_PROCESS_PRIVATE);
	return 0;
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
	 */
	pthread_spin_lock(&session_lock);
	bit = find_next_zero_bit(session_id_map, NR_MAX_SESSIONS_PER_NODE, 0);
	if (bit >= NR_MAX_SESSIONS_PER_NODE)
		goto unlock;

	__set_bit(bit, session_id_map);
	ses = session_net_map + bit;

	/* generic session init */
	memset(ses, 0, sizeof(*ses));
	set_local_session_id(ses, bit);
	ses->flags |= SESSION_NET_FLAGS_ALLOCATED;

	dprintf_DEBUG("sessiond_net id %d\n", bit);
unlock:
	pthread_spin_unlock(&session_lock);
	return ses;
}

void free_session(struct session_net *ses)
{
	unsigned int session_id;

	session_id = ses - session_net_map;
	if (session_id >= NR_MAX_SESSIONS_PER_NODE) {
		dprintf_ERROR("error session_net %#lx\n",
			(unsigned long)ses);
		return;
	}

	if (session_id == LEGOMEM_MGMT_SESSION_ID) {
		dprintf_ERROR("mgmt session cannot be freed %d", 0);
		return;
	}

	pthread_spin_lock(&session_lock);
	if (!__test_and_clear_bit(session_id, session_id_map)) {
		printf("%s(): WARN Session ID %d was free\n",
			__func__, session_id);
	}
	ses->flags = 0;
	pthread_spin_unlock(&session_lock);
}

void dump_net_sessions(void)
{
	int bkt;
	struct session_net *ses;
	struct board_info *bi;
	char ip_str[INET_ADDRSTRLEN];
	char ip_port_str[20];

	printf("bucket   ses_local    ses_remote   ip:port_remote       remote_name\n"); 
	printf("-------- ------------ ------------ -------------------- ------------------------------\n");

	pthread_spin_lock(&session_lock);
	for (bkt = 0; bkt < NR_MAX_SESSIONS_PER_NODE; bkt++) {
		ses = session_net_map + bkt;
		if (!(ses->flags & SESSION_NET_FLAGS_ALLOCATED))
			continue;

		bi = ses->board_info;
		get_ip_str(ses->board_ip, ip_str);
		sprintf(ip_port_str, "%s:%d", ip_str, bi->udp_port);

		printf("%-8d %-12u %-12u %-20s %-30s\n",
			bkt,
			get_local_session_id(ses),
			get_remote_session_id(ses),
			ip_port_str,
			bi->name);
	}
	pthread_spin_unlock(&session_lock);
}
