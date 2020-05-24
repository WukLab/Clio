/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * ctrl.c describes functions handling requests sent from on-board modules.
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
		return;
	}

	pid -= NR_MAX_USER_PID;
	pthread_spin_lock(&sys_pid_lock);
	if (!__test_and_clear_bit(pid, sys_pid_map))
		BUG();
	pthread_spin_unlock(&sys_pid_lock);
}

/*
 * Used by both ctrl and data path alloc handlers.
 * This is specially designed for multiverion on-chip modules.
 */
unsigned long __handle_ctrl_alloc(struct proc_info *pi, size_t size)
{
	unsigned int vregion_idx;
	struct vregion_info *vi;
	unsigned long addr;

	/*
	 * We are taking a shortcut here.
	 * Should be okay, still O(1).
	 */
	vregion_idx = pi->cached_vregion_index;

repeat:
	vi = index_to_vregion(pi, vregion_idx);

	/*
	 * Always populate all the pgtables.
	 * In case page fifo is not filled.
	 */
	addr = alloc_va_vregion(pi, vi, size, LEGOMEM_VM_FLAGS_POPULATE);
	if (unlikely(IS_ERR_VALUE(addr))) {
		vregion_idx++;
		if (vregion_idx == NR_VREGIONS) {
			dprintf_ERROR("Well OOM%d\n", 0);
			return 0;
		}
		goto repeat;
	}

	pi->cached_vregion_index = vregion_idx;
	return addr;
}

/*
 * rx->param32: size in bytes
 * rx->param8: pid
 * tx->param32: va
 *
 * On success, return the allocated VA via param32. Otherwise, return 0.
 * Sister function is board_soc_handle_alloc_free.
 */
static void handle_ctrl_alloc(struct lego_mem_ctrl *rx,
			      struct lego_mem_ctrl *tx)
{
	pid_t pid;
	unsigned int size;
	struct proc_info *pi;
	unsigned long addr;

	/* See comment at handle_ctrl_create_proc */
	pid = rx->param8 + NR_MAX_USER_PID;
	size = rx->param32;

	/* Prepare tx */
	tx->epid = 3;
	tx->addr = rx->addr;
	tx->cmd = rx->cmd;

	pi = get_proc_by_pid(pid);
	if (!pi) {
		dprintf_ERROR("pid %u %u. proc_info not found.\n",
			pid, pid - NR_MAX_USER_PID);
		tx->param32 = 0;
		goto out;
	}

	addr = __handle_ctrl_alloc(pi, size);
	tx->param32 = (u32)addr;

	dprintf_INFO("pid %d size %x addr %#x\n", pid, size, tx->param32);
out:
	dma_ctrl_send(tx, sizeof(*tx));
}

/*
 * rx->param32: addr
 * rx->param8: pid
 */
static void handle_ctrl_free(struct lego_mem_ctrl *rx,
			     struct lego_mem_ctrl *tx)
{
	/*
	 * 40bits are not enough to carry
	 * pid, addr, len at once.
	 * Thus for now I couldn't impl this.
	 */
	dprintf_ERROR("Not implemented%d\n", 0);
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

	/* Prepare tx */
	tx->epid = 3;
	tx->addr = rx->addr;
	tx->cmd = rx->cmd;

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

	/*
	 * When the module wants to do alloc,
	 * we only have 8bit for pid. Thus we
	 * have to cut the base.
	 */
	tx->param32 = pid - NR_MAX_USER_PID;

	dprintf_INFO("PID %u\n", tx->param32);
out:
	dma_ctrl_send(tx, sizeof(*tx));
}


static int __i = 0;

static void handle_kvs_alloc(struct lego_mem_ctrl *rx,
			     struct lego_mem_ctrl *tx)
{
	// memset?
	unsigned long va;
	int id;

	id = __i++;

	tx->epid = 3;
	tx->addr = 1;
	tx->param32 = id * 256;
	dma_ctrl_send(tx, sizeof(*tx));

	if (rx->cmd == CMD_LEGOMEM_KVS_ALLOC_BOTH) {
		id = __i++;
		va = fpga_mem_start_soc_va + id * 256;

		for (int i = 0; i < (256/8); i++) {
			*(uint64_t *)(va + i * 8) = 0;
		}

		tx->epid = 3;
		tx->addr = 0;
		tx->param32 = id * 256;
		dma_ctrl_send(tx, sizeof(*tx));
	}
}

__used
static void prepare_kvs(struct lego_mem_ctrl *rx, struct lego_mem_ctrl *tx)
{
	u64 *p;
	int i, cnt;

	/* Clear the first 16M hashtable */
	cnt = (16 * 1024 * 1024) / 8;
	p = (u64 *)fpga_mem_start_soc_va;
	for (i = 0; i < cnt; i++) {
		*p++ = 0;
	}

	/* Registers */
	tx->epid = 3;
	tx->addr = 0xff;
	tx->cmd = 0;
	tx->param8 = 0x5;
	tx->param32 = _FPGA_MEMORY_MAP_DATA_START;
	dma_ctrl_send(tx, sizeof(*tx));
}

__used
static void prepare_multiversion(struct lego_mem_ctrl *rx, struct lego_mem_ctrl *tx)
{
	handle_ctrl_create_proc(rx, tx);
}

/*
 * Prepare a usable VA range for on-board 100G testing module.
 * We need to allocate PID and VA ranges. While the testing module
 * can directly use them.
 */
__used
static void prepare_100g_test(void)
{
	struct proc_info *pi;
	pid_t pid;
	unsigned long addr, size;
	struct vregion_info *vi;
	int vregion_idx;

	pid = 1;
	pi = alloc_proc(pid, NULL, 0);
	if (!pi) {
		dprintf_ERROR("Cannot allocate PID %d for testing\n", pid);
		return;
	}

	size = PAGE_SIZE;
	vregion_idx = 0;
	vi = index_to_vregion(pi, vregion_idx);
	addr = alloc_va_vregion(pi, vi, size, LEGOMEM_VM_FLAGS_POPULATE);
	if (IS_ERR_VALUE(addr)) {
		dprintf_ERROR("Cannot allocate va vregion %d\n", vregion_idx);
		return;
	}

	dprintf_INFO("Testing module is safe to use [%#lx %#lx]\n",
		addr, addr + size);
}

/*
 * This is the CTRL AXIS DMA FIFO polling thread.
 * It will dispatch events to different handlers
 * depending on the rx->addr field.
 */
static void *ctrl_poll_func(void *_unused)
{
	struct lego_mem_ctrl *rx;
	struct lego_mem_ctrl *tx;
	axidma_dev_t dev;

	/* Allocate DMA-able send/recve buffers */
	dev = legomem_dma_info.dev;
	rx = axidma_malloc(dev, CTRL_BUFFER_SIZE);
	tx = axidma_malloc(dev, CTRL_BUFFER_SIZE);

#if 0
	prepare_multiversion(rx, tx);
	prepare_kvs(rx, tx);
	prepare_100g_test();
#endif

	while (1) {
		while (dma_ctrl_recv_blocking(rx, CTRL_BUFFER_SIZE) < 0)
			;

		dprintf_INFO("ADDR %x CMD %x EPID %x\n", rx->addr, rx->cmd, rx->epid);

		switch (rx->cmd) {
		case CMD_LEGOMEM_CTRL_CREATE_PROC:
			dprintf_INFO("create proc %d\n", 0);
			handle_ctrl_create_proc(rx, tx);
			break;
		case CMD_LEGOMEM_CTRL_ALLOC:
			dprintf_INFO("ctel alloc %d\n", 0);
			handle_ctrl_alloc(rx, tx);
			break;
		case CMD_LEGOMEM_CTRL_FREE:
			dprintf_INFO("ctrl free%d\n", 0);
			handle_ctrl_free(rx, tx);
			break;
		case CMD_LEGOMEM_KVS_ALLOC:
		case CMD_LEGOMEM_KVS_ALLOC_BOTH:
			dprintf_INFO("kvs alloc %d\n", 0);
			handle_kvs_alloc(rx, tx);
			break;
		default:
			dprintf_ERROR("Unknow cmd %#x\n", rx->cmd);
			break;
		}

#if 0
		switch (rx->addr) {
		case LEGOMEM_CTRL_ADDR_FREEPAGE_0:
		case LEGOMEM_CTRL_ADDR_FREEPAGE_1:
		case LEGOMEM_CTRL_ADDR_FREEPAGE_2:
			dprintf_INFO("freepage ack %d\n", 0);
			handle_ctrl_freepage_ack(rx, tx);
			break;
		default:
			switch (rx->cmd) {
			case CMD_LEGOMEM_CTRL_CREATE_PROC:
				dprintf_INFO("create proc %d\n", 0);
				handle_ctrl_create_proc(rx, tx);
				break;
			case CMD_LEGOMEM_CTRL_ALLOC:
				dprintf_INFO("ctel alloc %d\n", 0);
				handle_ctrl_alloc(rx, tx);
				break;
			case CMD_LEGOMEM_CTRL_FREE:
				dprintf_INFO("ctrl free%d\n", 0);
				handle_ctrl_free(rx, tx);
				break;
			case CMD_LEGOMEM_KVS_ALLOC:
			case CMD_LEGOMEM_KVS_ALLOC_BOTH:
				dprintf_INFO("kvs alloc %d\n", 0);
				handle_kvs_alloc(rx, tx);
				break;
			default:
				dprintf_ERROR("Unknow cmd %#x\n", rx->cmd);
				break;
			}
			break;
		}
#endif
	}
	return NULL;
}

int init_ctrl_polling(void)
{
	int ret;
	pthread_t t;

	ret = pthread_create(&t, NULL, ctrl_poll_func, NULL);
	if (ret) {
		dprintf_ERROR("Fail to launch CTRL poll func %d\n", ret);
		exit(1);
	}
	return 0;
}
