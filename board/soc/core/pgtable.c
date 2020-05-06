/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/rbtree.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <fpga/lego_mem_ctrl.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "core.h"
#include "pgtable.h"

/*
 * This is the BUS address for the pgtable in DRAM.
 */
#define FPGA_DRAM_PGTABLE_BASE	(0x550000000UL)
#define FPGA_DRAM_PGTABLE_SIZE	(0x10000UL)
int devmem_fd;
void *devmem_pgtable_base;

// TODO Zhiyuan
static __always_inline struct lego_mem_pte *
addr_to_pte(struct lego_mem_pte *pgtable,
	    unsigned long addr, unsigned long page_size)
{
	return NULL;
}

static void zap_pte(struct lego_mem_pte *pte,
		    unsigned long page_size)
{
	unsigned long phys;
	int order;

	if (unlikely(!pte->allocated)) {
		dprintf_ERROR("pte %#lx invalid\n",
			(unsigned long)pte);
		return;
	}

	if (pte->valid) {
		phys = pte->ppa;
		order = get_order(page_size);
		free_pfn(PHYS_PFN(phys), order);

		pte->allocated = 0;
		pte->valid = 0;
	}
}

/*
 * Called during legomem_free.
 * Called to free the physical pages for va range [start, end].
 */
void free_fpga_pte_range(struct proc_info *pi,
			 unsigned long start, unsigned long end,
			 unsigned long page_size)
{
#if 0
	struct lego_mem_pte *pgtable, *pte;

	pgtable = (struct lego_mem_pte *)pi->pgtable;

	while (start < end) {
		pte = addr_to_pte(pgtable, start, page_size);
		zap_pte(pte, page_size);
		start += page_size;
	}
#endif
}

static inline void alloc_pte(struct lego_mem_pte *pte,
			     unsigned long page_size)
{
	pte->allocated = 1;
	pte->valid = 0;
}

/*
 * Called during legomem_alloc.
 *
 * Update the PTEs that [start, start+len] mapped to.
 * We will mark the PTEs as Allocated, so the page fault handler
 * can know whether a certain VA was allocated by checking this bit
 * (similar to find_vma in linux ;/). But we do not allocate physical
 * memory at this point, that's passed along the free page list.
 */
void alloc_fpga_pte_range(struct proc_info *pi,
			  unsigned long start, unsigned long end,
			  unsigned long vm_flags, unsigned long page_size)
{
#if 0
	struct lego_mem_pte *pgtable, *pte;

	pgtable = (struct lego_mem_pte *)pi->pgtable;

	while (start < end) {
		pte = addr_to_pte(pgtable, start, page_size);
		alloc_pte(pte, page_size);
		start += page_size;
	}
#endif
}

/*
 * Called when a new process is created.
 * We need to prepare the pagetable for it.
 */
void setup_proc_fpga_pgtable(struct proc_info *pi)
{
	pi->pgtable = devmem_pgtable_base;
}

/*
 * Called once during startup to init anything related to fpga pgtables.
 * 
 * May 4, 2020 YS: now we only have a single pagetable for all processes,
 * thus we need to do mmap at beginging. If later on we implemented
 * per-process pgtable, this func is probably not necessary. Instead,
 * we need to move the mmap code to setup_proc_fpga_pgtable.
 */
void init_fpga_pgtable(void)
{
	devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (devmem_fd < 0) {
		perror("open");
		dprintf_ERROR("Fail to open /dev/mem ret %d\n", errno);
		exit(1);
	}

	devmem_pgtable_base = mmap(0,
				   FPGA_DRAM_PGTABLE_SIZE,
				   PROT_READ | PROT_WRITE,
				   MAP_SHARED, devmem_fd,
				   FPGA_DRAM_PGTABLE_BASE);
	if (devmem_pgtable_base == MAP_FAILED) {
		perror("mmap");
		dprintf_ERROR("Fail to mmap /dev/mem ret %d\n", errno);
		exit(1);
	}

	dprintf_INFO("/dev/mem fd=%d, VA[%#lx - %#lx] -> PA[%#lx - %#lx]\n",
		devmem_fd, (unsigned long)devmem_pgtable_base,
		(unsigned long)devmem_pgtable_base + FPGA_DRAM_PGTABLE_SIZE,
		FPGA_DRAM_PGTABLE_BASE, FPGA_DRAM_PGTABLE_BASE + FPGA_DRAM_PGTABLE_SIZE);
}
