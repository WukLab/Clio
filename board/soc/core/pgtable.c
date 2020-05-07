/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/rbtree.h>
#include <uapi/opcode.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <fpga/lego_mem_ctrl.h>
#include <fpga/pgtable.h>

#include "core.h"
#include "pgtable.h"

int devmem_fd;
void *devmem_pgtable_base;
void *devmem_pgtable_limit;

static inline struct lego_mem_pte *
shadow_to_fpga_pte(struct proc_info *pi, struct lego_mem_pte *soc_shadow_pte)
{
	unsigned long offset;
	struct lego_mem_pte *fpga_pte;

	offset = (unsigned long)soc_shadow_pte - (unsigned long)pi->soc_shadow_pgtable;
	if (unlikely(offset >= FPGA_DRAM_PGTABLE_SIZE)) {
		dprintf_ERROR("offset: %#lx max: %#lx\n",
			offset, FPGA_DRAM_PGTABLE_SIZE);
		return NULL;
	}
	fpga_pte = (struct lego_mem_pte *)((unsigned long)(pi->fpga_pgtable) + offset);
	return fpga_pte;
}

/* Free a PTE entry */
static void zap_pte(struct proc_info *pi, 
		    struct lego_mem_pte *soc_shadow_pte, unsigned long page_size)
{
	unsigned long phys = 0;
	int order;
	struct lego_mem_pte *fpga_pte;

	fpga_pte = shadow_to_fpga_pte(pi, soc_shadow_pte);
	if (!fpga_pte)
		return;

	if (unlikely(!fpga_pte->allocated)) {
		dprintf_ERROR("fpga pte %#lx invalid\n",
			(unsigned long)fpga_pte);
		return;
	}

	/*
	 * Valid means a page fault has ocurred on this page,
	 * thus we need to free its physical page.
	 */
	if (fpga_pte->valid) {
		phys = fpga_pte->ppa;
		order = get_order(page_size);
		free_pfn(PHYS_PFN(phys), order);

		fpga_pte->valid = 0;
	}
	fpga_pte->allocated = 0;
	fpga_pte->tag = 0;

#if 1
	dprintf_DEBUG("fpga PTE (%#lx): pa=%#lx\n", (u64)fpga_pte, phys);
#endif
}

/*
 * Called during legomem_free.
 * Called to free the physical pages for va range [start, end].
 */
void free_fpga_pte_range(struct proc_info *pi,
			 unsigned long start, unsigned long end,
			 unsigned long page_size)
{
	struct lego_mem_pte *pte;

	for ( ; start < end; start += page_size) {
		pte = addr_to_shadow_pte(pi, start, PAGE_SHIFT);
		if (!pte) {
			dprintf_ERROR("addr %#lx PTE cannot find. "
				      "It should have been setup by alloc. "
				      "Somebody caused memory corruption. \n", start);
			continue;
		}

		zap_shadow_pte(pte);
		zap_pte(pi, pte, page_size);
	}
}

static inline struct lego_mem_pte *
alloc_one_fpga_pte(struct proc_info *pi, struct lego_mem_pte *soc_shadow_pte,
		   unsigned long addr, unsigned long vm_flags, unsigned long page_size)
{
	struct lego_mem_pte *fpga_pte;
	unsigned long pfn = 0;
	int order;

	fpga_pte = shadow_to_fpga_pte(pi, soc_shadow_pte);
	if (!fpga_pte)
		return NULL;

	if (unlikely(fpga_pte->allocated)) {
		dprintf_ERROR("fpga pte was allocated. pte %#lx\n",
			(unsigned long)fpga_pte);
		return NULL;
	}

	fpga_pte->allocated = 1;
	fpga_pte->tag = soc_shadow_pte->tag;

	/*
	 * Did user ask to pre-populate PTEs
	 * to avoid further pgfaults?
	 */
	if (vm_flags & LEGOMEM_VM_FLAGS_POPULATE) {
		order = get_order(page_size);
		pfn = alloc_pfn(order);
		if (!pfn) {
			dprintf_ERROR("User asked to prepopulate pgtables. \n"
				      "But we are running out of memory! %d\n", 0);
		} else {
			fpga_pte->ppa = PFN_PHYS(pfn);
		}
		fpga_pte->valid = 1;
	} else
		fpga_pte->valid = 0;

#if 1
	dprintf_DEBUG("fpga PTE (%#lx): pa=%#lx (buddy_pfn=%#lx) tag=%#lx allocated=%d\n",
		(u64)fpga_pte,
		(u64)fpga_pte->ppa, pfn, fpga_pte->tag, fpga_pte->allocated);
#endif
	return fpga_pte;
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
	struct lego_mem_pte *pte;

	// XXX
	vm_flags = LEGOMEM_VM_FLAGS_POPULATE;

	dprintf_DEBUG("pid: %u [%#lx - %#lx] vm_flags: %#lx\n",
		pi->pid, start, end, vm_flags);

	for ( ; start < end; start += page_size) {
		/*
		 * We will find a free slot from the SoC shadow copy
		 * Once found one, we will set the corresponding PTE in FPGA DRAM.
		 */
		pte = alloc_one_shadow_pte(pi, start, PAGE_SHIFT);
		if (!pte) {
			dprintf_ERROR("Fail to alloc shadow PTE. adddr%#lx\n", start);
			continue;
		}

		alloc_one_fpga_pte(pi, pte, start, vm_flags, page_size);
	}
}

void handle_test_pte(struct thpool_buffer *tb)
{
	struct legomem_test_pte *rx;
	struct op_test_pte *op;
	struct legomem_common_headers *tx;
	struct proc_info *pi;
	pid_t pid;

	rx = (struct legomem_test_pte *)tb->rx;
	op = &rx->op;
	tx = (struct legomem_common_headers *)tb->tx;
	set_tb_tx_size(tb, sizeof(*tx));

	/* Create one for testing */
	pid = op->pid;
	pi = alloc_proc(pid, NULL, 0);
	dump_procs();

	dprintf_INFO("PTE TEST pid %u op: %s [%#lx - %#lx]\n",
		op->pid,
		op->op == OP_TEST_PTE_ALLOC ? "alloc" : "free",
		op->start, op->end);

	if (op->op == OP_TEST_PTE_ALLOC)
		alloc_fpga_pte_range(pi, op->start, op->end, 0, PAGE_SIZE);
	else
		free_fpga_pte_range(pi, op->start, op->end, PAGE_SIZE);
}

/*
 * Called when a new process is created.
 * We need to prepare the pagetable for it.
 */
void setup_proc_fpga_pgtable(struct proc_info *pi)
{
	pi->fpga_pgtable = devmem_pgtable_base;
	pi->soc_shadow_pgtable = soc_shadow_pgtable;

	dprintf_DEBUG("Setup pid: %u fpga_pgtable: %#lx soc_shadow_pgtable: %#lx\n",
		pi->pid, (u64)pi->fpga_pgtable, (u64)pi->soc_shadow_pgtable);
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

	soc_shadow_pgtable = malloc(FPGA_DRAM_PGTABLE_SIZE);
	if (!soc_shadow_pgtable) {
		dprintf_ERROR("Fail to alloc shadow page table %d\n", errno);
		exit(1);
	}

#if 1
	test_pgtable_access();
#endif

	/*
	 * Reset both hashtables to avoid
	 * garbage values. Also. memset does not work. :/
	 */
#if 0
	memset((void *)devmem_pgtable_base, 0, FPGA_DRAM_PGTABLE_SIZE);
#else
	unsigned long *ptr = devmem_pgtable_base;
	while (ptr < (unsigned long *)devmem_pgtable_limit)
		*ptr++ = 0;
#endif
	memset((void *)soc_shadow_pgtable, 0, FPGA_DRAM_PGTABLE_SIZE);

	devmem_pgtable_limit = devmem_pgtable_base + FPGA_DRAM_PGTABLE_SIZE;

	dprintf_INFO("/dev/mem fd=%d\n", devmem_fd);

	dprintf_INFO("FPGA pgtable VA[%#lx - %#lx] -> PA[%#lx - %#lx]\n",
		(unsigned long)devmem_pgtable_base, (unsigned long)devmem_pgtable_limit,
		FPGA_DRAM_PGTABLE_BASE, FPGA_DRAM_PGTABLE_BASE + FPGA_DRAM_PGTABLE_SIZE);

	dprintf_INFO("Shadow pgtable VA[%#lx - %#lx]\n",
		(unsigned long)soc_shadow_pgtable,
		(unsigned long)(soc_shadow_pgtable + FPGA_DRAM_PGTABLE_SIZE));
}
