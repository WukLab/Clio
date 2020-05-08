/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

/*
 * The purpose of shadow copy is to accelerate the
 * slot searching when soc wants to allocate a new slot entry from a bucket.
 * It also accelerates when soc vma code wants to check if certain VA
 * range can be allocated with enough slots left..
 *
 * The shadow copy and fpga pgtable might diverge during runtime, i.e.,
 * fpga pgtable will have valid+ppa set after pgtables.
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

void *soc_shadow_pgtable;

struct conflict_vma conflict_vmas[FPGA_NUM_PGTABLE_BUCKETS];

static inline struct conflict_vma *to_conflict_vma_head(int index)
{
	BUG_ON(index >= FPGA_NUM_PGTABLE_BUCKETS);
	return &conflict_vmas[index];
}

int add_conflict_vma(int bkt_index, struct vm_area_struct *vma)
{
	struct conflict_vma *cv;

	cv = to_conflict_vma_head(bkt_index);

	pthread_spin_lock(&cv->lock);
	list_add(&vma->conflict_list, &cv->head);
	pthread_spin_unlock(&cv->lock);

	return 0;
}

void remove_all_conflict_vmas(int bkt_index)
{
	struct conflict_vma *cv;
	struct vm_area_struct *vma;

	cv = to_conflict_vma_head(bkt_index);

	pthread_spin_lock(&cv->lock);
	while (!list_empty(&cv->head)) {
		vma = list_entry(cv->head.next, struct vm_area_struct, conflict_list);
		list_del(&vma->conflict_list);
		cv->nr--;
	}
	pthread_spin_unlock(&cv->lock);
}

/*
 * It's a shadow copy, thus the same offset.
 * We first calculate the offset of @fpga_pte from fpga table base,
 * then we get its shadow pte from soc table.
 */
static inline struct lego_mem_pte *
to_soc_shadow_pte(struct proc_info *pi, struct lego_mem_pte *fpga_pte)
{
	u64 offset;
	struct lego_mem_pte *soc_shadow_pte;

	offset = (u64)fpga_pte - (u64)(pi->fpga_pgtable);
	if (unlikely(offset >= FPGA_DRAM_PGTABLE_SIZE)) {
		dprintf_ERROR("offset: %#lx max: %#lx\n",
			offset, FPGA_DRAM_PGTABLE_SIZE);
		return NULL;
	}

	soc_shadow_pte = (struct lego_mem_pte *)((u64)(pi->soc_shadow_pgtable) + offset);
	return soc_shadow_pte;
}

/*
 * Sister function for zap_pte().
 * Clear a shadow PTE.
 */
void zap_shadow_pte(struct lego_mem_pte *pte)
{
	if (unlikely(!pte->allocated)) {
		dprintf_ERROR("shadow pte %#lx invalid\n",
			(unsigned long)pte);
		return;
	}
	pte->tag = 0;
	pte->allocated = 0;
	pte->valid = 0;
}

/*
 * Sister function for alloc_one_pte．
 * Allocate a new slot from the SoC's shadow pgtable.
 */
struct lego_mem_pte *
alloc_one_shadow_pte(struct proc_info *pi, unsigned long addr,
		     unsigned long page_size_shift)
{
	struct lego_mem_pte *pte;
	struct lego_mem_bucket *bucket;
	int i;

	bucket = addr_to_bucket(pi->soc_shadow_pgtable, addr, page_size_shift);

	/*
	 * Walk through all slots within this bucket,
	 * try to find a free slot.
	 */
	for (i = 0; i < FPGA_NUM_PTE_PER_BUCKET; i++) {
		pte = &bucket->ptes[i];

#if 0
		dprintf_DEBUG("PTE IN BUCKET: Bucket va: %#lx. pte=%#lx allocated: %d\n",
			(u64)bucket, (u64)pte, pte->allocated);
#endif

		if (pte->allocated)
			continue;

		pte->tag = generate_tag(pi->pid, addr);
		pte->allocated = 1;
		pte->valid = 0;
		return pte;
	}
	return NULL;
}

int init_shadow_pgtable(void)
{
	int i;
	struct conflict_vma *cv;

	for (i = 0; i < FPGA_NUM_PGTABLE_BUCKETS; i++) {
		cv = to_conflict_vma_head(i);
		pthread_spin_init(&cv->lock, PTHREAD_PROCESS_PRIVATE);
	}

	return 0;
}
