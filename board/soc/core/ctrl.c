/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * Ctrl.c describes functions handling requests sent from on-board modules.
 * For instance, requests from extended modules and so on.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>
#include <uapi/thpool.h>
#include <uapi/bitops.h>
#include <uapi/lego_mem.h>
#include <fpga/lego_mem_ctrl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>

#include "dma.h"
#include "core.h"

/*
 * To save memory, we only allocate the range we responsible for.
 * We will adjust during alloc/free time.
 */
#define NR_SYSTEM_PID	(NR_MAX_SYSTEM_PID - NR_MAX_USER_PID)
static DECLARE_BITMAP(sys_pid_map, NR_SYSTEM_PID);
static pthread_spinlock_t(sys_pid_lock);

static int alloc_sys_pid(void)
{
	int bit;

	pthread_spin_lock(&sys_pid_lock);
	bit = find_next_zero_bit(sys_pid_map, NR_SYSTEM_PID, 1);
	if (bit >= NR_SYSTEM_PID) {
		bit = -1;
		goto unlock;
	}
	__set_bit(bit, sys_pid_map);

unlock:
	pthread_spin_unlock(&sys_pid_lock);
	return bit + NR_MAX_USER_PID;
}

static void free_sys_pid(int pid)
{
	if (pid < NR_MAX_USER_PID || pid >= NR_MAX_SYSTEM_PID) {
		dprintf_ERROR("Invalid PID: %d\n", pid);
	}

	pid -= NR_MAX_USER_PID;
	pthread_spin_lock(&sys_pid_lock);
	if (!__test_and_clear_bit(pid, sys_pid_map))
		BUG();
	pthread_spin_unlock(&sys_pid_lock);
}

static void handle_ctrl_alloc(struct lego_mem_ctrl *rx,
			      struct lego_mem_ctrl *tx)
{

}

static void handle_ctrl_free(struct lego_mem_ctrl *rx,
			     struct lego_mem_ctrl *tx)
{

}

/*
 * This func handles the case when on-board modules wish
 * to create a process context and virtual memory system.
 * Sister function: handle_create_proc.
 *
 * Return the allocated PID via tx->param32.
 */
static void handle_ctrl_create_proc(struct lego_mem_ctrl *rx,
				    struct lego_mem_ctrl *tx)
{
	struct proc_info *pi;
	pid_t pid;

	pid = alloc_sys_pid();
	if (pid < 0) {
		dprintf_ERROR("System PID space full. %d\n", 0);
		tx->param32 = 0;
		goto out;
	}

	pi = alloc_proc(pid, NULL, 0);
	if (!pi) {
		dprintf_ERROR("Fail to alloc_proc. OOM %d\n", 0);
		free_sys_pid(pid);
		tx->param32 = 0;
		goto out;
	}

	tx->param32 = pid;
out:
	dma_ctrl_send(tx, sizeof(*tx));
}
