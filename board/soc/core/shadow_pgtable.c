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

unsigned long nr_allocated_ptes = 0;

struct conflict_info {
	struct list_head list;
	struct proc_info *pi;
	struct vregion_info *vi;
	unsigned long start;
	unsigned long page_size;
};

struct shadow_bucket_info {
	atomic_int nr_free;
	int nr_conflicts;
	struct list_head conflict_head;
	pthread_spinlock_t lock;
};

struct shadow_bucket_info shadow_bucket_infos[FPGA_NUM_PGTABLE_BUCKETS];

static inline struct shadow_bucket_info *
index_to_shadow_bucket_info(int index)
{
	if (unlikely(index >= FPGA_NUM_PGTABLE_BUCKETS)) {
		dprintf_ERROR("index = %d max = %ld\n",
			index, FPGA_NUM_PGTABLE_BUCKETS);
		BUG();
	}
	return &shadow_bucket_infos[index];
}

/*
 * Return true if there is conflict and the conflict info is saved.
 * Otherwise return false, caller can proceed.
 */
bool check_and_insert_shadow_conflicts(struct proc_info *pi,
				       struct vregion_info *vi,
				       unsigned long addr,
				       unsigned long page_size,
				       unsigned long page_size_shift,
				       unsigned long len)
{
	unsigned long start, end;
	struct shadow_bucket_info *info;
	int index;
	bool has_conflict;

	/*
	 * Both addr and len are page aligned,
	 * ensured by caller.
	 */
	start = addr;
	end = start + len;

	has_conflict = false;

	for ( ; start < end; start += page_size) {
		index = addr_to_bucket_index(start, page_size_shift);
		info = index_to_shadow_bucket_info(index);

		if (unlikely(atomic_load(&info->nr_free) == 0)) {
			struct conflict_info *ci;

			ci = malloc(sizeof(*ci));
			ci->pi = pi;
			ci->vi = vi;
			ci->start = start;
			ci->page_size = page_size;

			pthread_spin_lock(&info->lock);
			list_add(&ci->list, &info->conflict_head);
			info->nr_conflicts++;
			pthread_spin_unlock(&info->lock);

			/*
			 * Mark this page in the VMA tree.
			 */
			vma_tree_new(pi, vi, start, page_size, LEGOMEM_VM_FLAGS_CONFLICT);

			dprintf_DEBUG("New conflict. index=%d insert: pid=%d addr=%#lx nr=%d\n",
				index, pi->pid, start, info->nr_conflicts);

			has_conflict = true;
		}
	}
	return has_conflict;
}

static void free_all_conflicts(struct shadow_bucket_info *info)
{
	struct conflict_info *ci;

	pthread_spin_lock(&info->lock);
	while (!list_empty(&info->conflict_head)) {
		ci = list_entry(info->conflict_head.next, struct conflict_info, list);
		list_del(&ci->list);
		info->nr_conflicts--;

		dprintf_DEBUG("free conflict pid %d [%#lx-%#lx]\n",
			ci->pi->pid, ci->start, ci->start + ci->page_size);

		/*
		 * TODO
		 * the pi may already be closed. and because we don't have
		 * reference count, it was freed. We need to add that one day..
		 */

		/*
		 * This function runs deep within free_va_vregion stack.
		 * free_va_vregion
		 *  __free_va
		 *  ...
		 *   unmap_single_vma
		 *     free_fpga_pte_range
		 *       zap_shadow_pte
		 *
		 * And the vregion lock is held.
		 * Thus we must recursivelyc all __free_va.
		 */
		__free_va(ci->pi, ci->vi, ci->start, ci->page_size);
		free(ci);
	}
	pthread_spin_unlock(&info->lock);
}

/*
 * Sister function for zap_pte().
 * Clear a shadow PTE.
 */
void zap_shadow_pte(struct proc_info *pi, struct lego_mem_pte *pte,
		    unsigned long page_size)
{
	struct shadow_bucket_info *info;
	unsigned int index;

	if (unlikely(!pte->allocated)) {
		dprintf_ERROR("shadow pte %#lx invalid\n",
			(unsigned long)pte);
		return;
	}

	index = shadow_pte_to_bucket_index(pi, pte);
	info = index_to_shadow_bucket_info(index);

	atomic_fetch_add(&info->nr_free, 1);
	nr_allocated_ptes--;
	free_all_conflicts(info);

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
	struct shadow_bucket_info *info;
	unsigned int index;
	int i;

	bucket = addr_to_bucket(pi->soc_shadow_pgtable, addr, page_size_shift);

	index = addr_to_bucket_index(addr, page_size_shift);
	info = index_to_shadow_bucket_info(index);

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

		atomic_fetch_sub(&info->nr_free, 1);
		nr_allocated_ptes++;
		return pte;
	}
	return NULL;
}

void dump_shadow_pgtable_conflicts(void)
{
	/* int i; */
	/* struct shadow_bucket_info *info; */
        /*  */
	/* for (i = 0; i < FPGA_NUM_PGTABLE_BUCKETS; i++) { */
	/*         info = index_to_shadow_bucket_info(i); */
	/* } */
}

int init_shadow_pgtable(void)
{
	int i;
	struct shadow_bucket_info *info;

	for (i = 0; i < FPGA_NUM_PGTABLE_BUCKETS; i++) {
		info = index_to_shadow_bucket_info(i);

		atomic_store(&info->nr_free, FPGA_NUM_PTE_PER_BUCKET);

		info->nr_conflicts = 0;
		INIT_LIST_HEAD(&info->conflict_head);
		pthread_spin_init(&info->lock, PTHREAD_PROCESS_PRIVATE);
	}

	soc_shadow_pgtable = malloc(FPGA_MEMORY_MAP_PGTABLE_SIZE);
	if (!soc_shadow_pgtable) {
		dprintf_ERROR("Fail to alloc shadow page table %d\n", errno);
		exit(1);
	}
	memset((void *)soc_shadow_pgtable, 0, FPGA_MEMORY_MAP_PGTABLE_SIZE);

	dprintf_INFO("Shadow pgtable @[%#lx - %#lx]\n",
		(unsigned long)soc_shadow_pgtable,
		(unsigned long)(soc_shadow_pgtable + FPGA_MEMORY_MAP_PGTABLE_SIZE));

	dprintf_INFO("FPGA_NUM_PGTABLE_BUCKETS: %ld, FPGA_NUM_PTE_PER_BUCKET: %ld, total coverd physical size: %#lx\n",
		FPGA_NUM_PGTABLE_BUCKETS, FPGA_NUM_PTE_PER_BUCKET,
		(FPGA_NUM_PTE_PER_BUCKET * FPGA_NUM_PGTABLE_BUCKETS) * PAGE_SIZE);

	return 0;
}

void dump_shadow_pgtable_util(void)
{
	double util;

	util  = ((double)nr_allocated_ptes / (double)FPGA_NUM_TOTAL_PTES) * 100;
	printf("PTE Util: %ld / %ld, %lf \n", nr_allocated_ptes, FPGA_NUM_TOTAL_PTES, util);
}
