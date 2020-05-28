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

	/* Shift to SYSTEM PID start */
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
 * 
 * This is specially designed for multiverion on-chip modules.
 * This is also used by KVS virt alloc FIFOs.
 */
unsigned long __handle_ctrl_alloc(struct proc_info *pi, size_t size,
				  unsigned long vm_flags)
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
	 * _Always_ populate all the pgtables!
	 * In case page fifo is not filled, or we are doing on-board test.
	 * This is not normal use path anyway.
	 */
	addr = alloc_va_vregion(pi, vi, size, LEGOMEM_VM_FLAGS_POPULATE | vm_flags);
	if (unlikely(!addr)) {
		dprintf_DEBUG("PID %u vregion_idx %u is full, move to next one.\n",
			pi->pid, vregion_idx);

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

	/* XXX EPID Prepare tx */
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

	/*
	 * Get a pre-populated areas.
	 * No need to clear, i guess?
	 */
	addr = __handle_ctrl_alloc(pi, size, 0);
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

static pid_t __handle_ctrl_create_proc(void)
{
	struct proc_info *pi;
	pid_t pid;

	/*
	 * This returns a large SYSTEM PID
	 * Starting from [NR_MAX_USER_PID, NR_MAX_USER_PID).
	 */
	pid = alloc_sys_pid();
	if (pid < 0) {
		dprintf_ERROR("System PID space full. %d\n", 0);
		return 0;
	}

	/*
	 * Use the large System PID to insert into
	 * the hashtable to avoid conflict with normal
	 * monitor allocated PIDs (which start from 0..)
	 */
	pi = alloc_proc(pid, NULL, 0);
	if (!pi) {
		dprintf_ERROR("Fail to alloc_proc. OOM %d\n", 0);
		free_sys_pid(pid);
		return 0;
	}
	return pid;
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
	pid_t pid;

	/* XXX EPID Prepare tx */
	tx->epid = 3;
	tx->addr = rx->addr;
	tx->cmd = rx->cmd;

	pid = __handle_ctrl_create_proc();
	if (!pid) {
		tx->param32 = 0;
		goto out;
	}

	/*
	 * When the module wants to do alloc,
	 * we only have 8bit for pid. Thus we
	 * have to cut the base.
	 */
	tx->param32 = pid - NR_MAX_USER_PID;
	dprintf_DEBUG("PID %u\n", tx->param32);
out:
	dma_ctrl_send(tx, sizeof(*tx));
}


/*
 * The first portion is used by physical KVS hashtable.
 */
#define KVS_PHYS_HASHTABLE_SIZE	(4 * 1024 * 1024)
#define KVS_PHYS_ENTRY_SIZE	(1024)
#define EPID_KVS		(3)

__used
static void handle_kvs_alloc_phys(struct lego_mem_ctrl *rx,
			          struct lego_mem_ctrl *tx)
{
	unsigned long va;
	int id;
	static int __cached_kvs_phys_i = 0;

	id = __cached_kvs_phys_i++;

	/* FIFO 1 */
	tx->epid = EPID_KVS;
	tx->addr = 1;
	tx->param8 = 0;
	tx->param32 = id * KVS_PHYS_ENTRY_SIZE + KVS_PHYS_HASHTABLE_SIZE;
	dma_ctrl_send(tx, sizeof(*tx));

	if ((id % 1000) == 0)
		dprintf_DEBUG("alloc fifo1: tx->param32 addr %#x\n", tx->param32);

	if (rx->cmd == CMD_LEGOMEM_KVS_ALLOC_BOTH) {
		id = __cached_kvs_phys_i++;

		/* Memset this entry */
		va = fpga_mem_start_soc_va + _FPGA_MEMORY_MAP_DATA_START + id * KVS_PHYS_ENTRY_SIZE;
		clear_fpga_page((void *)va, KVS_PHYS_ENTRY_SIZE);

		/* FIFO 0 */
		tx->epid = EPID_KVS;
		tx->addr = 0;
		tx->param8 = 0;
		tx->param32 = id * KVS_PHYS_ENTRY_SIZE + KVS_PHYS_HASHTABLE_SIZE;
		dma_ctrl_send(tx, sizeof(*tx));
	}
}

__used
static void prepare_kvs_phys(struct lego_mem_ctrl *rx, struct lego_mem_ctrl *tx)
{
	/*
	 * Note that fpga_mem_start_soc_va starts from FPGA memory 0.
	 * Thus we need to add the DATA offset, which is 1GB by default.
	 */
	clear_fpga_page((void *)(fpga_mem_start_soc_va + _FPGA_MEMORY_MAP_DATA_START),
			KVS_PHYS_HASHTABLE_SIZE);

	/* Registers */
	tx->epid = EPID_KVS;
	tx->addr = 0xff;
	tx->cmd = 0;
	tx->param8 = 0x5;
	tx->param32 = _FPGA_MEMORY_MAP_DATA_START;
	dma_ctrl_send(tx, sizeof(*tx));

	dprintf_INFO("\n\n\tDone preparing for KVS Phys... %d\n\n", 0);
}

/*
 * Per entry size
 * allocation granularity
 */
#define KVS_VIRT_ENTRY_SIZE	(1024)
#define KVS_VIRT_HASHTABLE_SIZE	(4*1024*1024)

/*
 * The avilable VA space for KVS virt
 */
#define KVS_VIRT_NR_PAGES	(64)
#define KVS_VIRT_VA_SIZE	(PAGE_SIZE * KVS_VIRT_NR_PAGES)

static pid_t kvs_virt_pid;
static struct proc_info *kvs_virt_pi;

__used
static void handle_kvs_alloc_virt(struct lego_mem_ctrl *rx,
			          struct lego_mem_ctrl *tx)
{
	unsigned int addr;	
	static int _cached_index = 0;

	addr = _cached_index * KVS_VIRT_ENTRY_SIZE + KVS_VIRT_HASHTABLE_SIZE;
	_cached_index++;

	if (addr >= KVS_VIRT_VA_SIZE) {
		dprintf_ERROR("_cached_index %d addr %#lx is full. Enlarge.\n",
			_cached_index, KVS_VIRT_VA_SIZE);
		exit(0);
	}

#if 0
	if ((_cached_index % 1)==0)
		dprintf_DEBUG("nr %d\n", _cached_index);
#endif

	/* FIFO 1 */
	tx->epid = EPID_KVS;
	tx->addr = 1;
	tx->param8 = 0;
	tx->param32 = addr;
	dma_ctrl_send(tx, sizeof(*tx));

	if (rx->cmd == CMD_LEGOMEM_KVS_ALLOC_BOTH) {
		addr = _cached_index * KVS_VIRT_ENTRY_SIZE + KVS_VIRT_HASHTABLE_SIZE;
		_cached_index++;

		/* FIFO 0 */
		tx->epid = EPID_KVS;
		tx->addr = 0;
		tx->param32 = 0;
		tx->param32 = addr;
		dma_ctrl_send(tx, sizeof(*tx));
	}
}

__used
static void prepare_kvs_virt(struct lego_mem_ctrl *rx, struct lego_mem_ctrl *tx)
{
	pid_t sys_pid;
	unsigned long addr, size;

	/* Get a System PID (not starting from 0) */
	sys_pid = __handle_ctrl_create_proc();
	if (!sys_pid) {
		dprintf_ERROR("Fail to prepare PID for KVS virt %d\n", 0);
		return;
	}
	kvs_virt_pid = sys_pid;
	kvs_virt_pi = get_proc_by_pid(kvs_virt_pid);
	BUG_ON(!kvs_virt_pi);

	tx->epid = EPID_KVS;
	tx->addr = 0xff;
	tx->cmd = 1;
	tx->param8 = 0;
	tx->param32 = kvs_virt_pid;
	dma_ctrl_send(tx, sizeof(*tx));
	dprintf_DEBUG("Prepared Sys PID=%u for KVS virt ...\n", kvs_virt_pid);

	sleep(2);

	/* VA Base */
	size = KVS_VIRT_VA_SIZE;
	addr = __handle_ctrl_alloc(kvs_virt_pi, size, LEGOMEM_VM_FLAGS_POPULATE | LEGOMEM_VM_FLAGS_ZERO);
	if (IS_ERR_VALUE(addr)) {
		dprintf_ERROR("Cannot allocate va vregion %d\n", 0);
		return;
	}

	tx->epid = EPID_KVS;
	tx->addr = 0xff;
	tx->cmd = 0;
	tx->param8 = 0;
	tx->param32 = addr;
	dma_ctrl_send(tx, sizeof(*tx));
	dprintf_DEBUG("Prepared vaddr tx->param32 [%#x - #%x] for KVS virt ... \n",
		(u32)tx->param32, (u32)(tx->param32 + size));

	dprintf_INFO("\n\n\tDone preparing for KVS Virt... %d\n\n", 0);
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

	size = PAGE_SIZE * 8;
	vregion_idx = 0;
	vi = index_to_vregion(pi, vregion_idx);
	addr = alloc_va_vregion(pi, vi, size, LEGOMEM_VM_FLAGS_POPULATE);
	if (!addr) {
		dprintf_ERROR("Cannot allocate va vregion %d\n", vregion_idx);
		return;
	}

	dprintf_INFO("\n\n"
		"\t Testing module is safe to use PID %u  VA@[%#lx-%#lx]\n\n",
		pid, addr, addr + size);
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

	/*
	 * Remember to change the handlers below
	 * Only one handler at a time..
	 */
	prepare_kvs_phys(rx, tx);
	//prepare_kvs_virt(rx, tx);

	//prepare_multiversion(rx, tx);
	//prepare_100g_test();

	while (1) {
		while (dma_ctrl_recv_blocking(rx, CTRL_BUFFER_SIZE) < 0)
			;

		//dprintf_INFO("addr=%x cmd=%x epid=%x\n", rx->addr, rx->cmd, rx->epid);

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
			handle_kvs_alloc_phys(rx, tx);
			//handle_kvs_alloc_virt(rx, tx);
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
				handle_kvs_alloc_phys(rx, tx);
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
