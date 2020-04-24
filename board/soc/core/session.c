/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
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

static DECLARE_BITMAP(session_id_map, NR_MAX_SESSIONS_PER_NODE);
static pthread_spinlock_t(session_id_lock);

__attribute__((constructor))
static void init_session_subsys(void)
{
	pthread_spin_init(&session_id_lock, PTHREAD_PROCESS_PRIVATE);
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
	if (!__test_and_clear_bit(session_id, session_id_map)) {
		printf("%s(): WARN Session ID %d was free\n",
			__func__, session_id);
	}
	pthread_spin_unlock(&session_id_lock);
}
