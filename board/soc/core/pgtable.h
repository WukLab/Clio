/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */
#ifndef _PGTABLE_H_
#define _PGTABLE_H_

#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/thpool.h>
#include <fpga/pgtable.h>
#include <fpga/lego_mem_ctrl.h>
#include <stdio.h>

#include "core.h"

/*
 * This is the BUS address for the pgtable in DRAM.
 */
#define FPGA_DRAM_PGTABLE_BASE	(0x520000000UL)
#define FPGA_DRAM_PGTABLE_SIZE	(0x1000000UL)

extern void *devmem_pgtable_base;
extern void *devmem_pgtable_limit;

void init_fpga_pgtable(void);

void setup_proc_fpga_pgtable(struct proc_info *pi);

void alloc_fpga_pte_range(struct proc_info *pi,
			  unsigned long start, unsigned long end,
			  unsigned long vm_flags, unsigned long page_size);
void free_fpga_pte_range(struct proc_info *pi,
			 unsigned long start, unsigned long end,
			 unsigned long page_size);

/* shadow pgtable */
extern void *soc_shadow_pgtable;
struct lego_mem_pte *
alloc_one_shadow_pte(struct proc_info *pi, unsigned long addr,
		     unsigned long page_size_shift);
void zap_shadow_pte(struct proc_info *pi, struct lego_mem_pte *pte,
		    unsigned long page_size);

// TODO replace this with lookup3
static inline unsigned long
hash_lower_bits(unsigned long fullTag, int shift)
{
	return (fullTag >> shift) & ((0x1UL << FPGA_BUCKET_NUMBER_LENGTH) - 1);
}

static inline unsigned int
addr_to_bucket_index(unsigned long addr, unsigned long page_size_shift)
{
	unsigned int index;

	/*
	 * TODO: Hash function
	 */
	index = hash_lower_bits(addr, page_size_shift);
	return index;
}

static inline struct lego_mem_bucket *
addr_to_bucket(struct lego_mem_pte *pgtable_base_addr,
	       unsigned long addr, unsigned long page_size_shift)
{
	struct lego_mem_bucket *bucket;
	unsigned int index;

	index = addr_to_bucket_index(addr, page_size_shift);
	bucket = (struct lego_mem_bucket *)(pgtable_base_addr) + index;

	/* Sanity check */
	if (unlikely((u64)bucket >= (u64)devmem_pgtable_limit)) {
		dprintf_ERROR("addr: %#lx index: %#x "
			      "bucket: %#lx base: %#lx limit: %#lx\n",
			      addr, index,
			      (unsigned long)bucket, (unsigned long)pgtable_base_addr,
			      (unsigned long)devmem_pgtable_limit);
		BUG();
	}

#if 0
	dprintf_DEBUG("base: %#lx Bucket va: %#lx. index=%#lx "
		      "Offset from base: %#lx\n",
		      (unsigned long)pgtable_base_addr,
		      (unsigned long)bucket, index,
		      ((unsigned long)bucket - (unsigned long)pgtable_base_addr));
#endif

	return bucket;
}

static inline unsigned long
generate_tag(pid_t pid, unsigned long va)
{
	unsigned long tag = ((unsigned long)(pid & 0xFFFF) << (64 - FPGA_TAG_OFFSET)) |
			(va >> FPGA_TAG_OFFSET);

#if 0
	dprintf_DEBUG("    Tag=%#lx addr=%#lx pid=%#x offset=%d\n",
			tag, va, pid, FPGA_TAG_OFFSET);
#endif
	return tag;
}

static inline struct lego_mem_pte *
addr_to_fpga_pte(struct proc_info *pi, unsigned long addr,
		 unsigned long page_size_shift)
{
	struct lego_mem_pte *pte;
	struct lego_mem_bucket *bucket;
	int i;
	u64 tag;

	bucket = addr_to_bucket(pi->fpga_pgtable, addr, page_size_shift);
	tag = generate_tag(pi->pid, addr);

	for (i = 0; i < FPGA_NUM_PTE_PER_BUCKET;i++) {
		pte = &bucket->ptes[i];
		if (pte->allocated && pte->tag == tag)
			return pte;
	}
	return NULL;
}

/*
 * Walk through the shadow hashtable,
 * find if @addr has a slot allocated before.
 */
static inline struct lego_mem_pte *
addr_to_shadow_pte(struct proc_info *pi, unsigned long addr,
		   unsigned long page_size_shift)
{
	struct lego_mem_pte *pte;
	struct lego_mem_bucket *bucket;
	u64 tag;
	int i;

	bucket = addr_to_bucket(pi->soc_shadow_pgtable, addr, page_size_shift);
	tag = generate_tag(pi->pid, addr);

	for (i = 0; i < FPGA_NUM_PTE_PER_BUCKET; i++) {
		pte = &bucket->ptes[i];
		if (pte->allocated && pte->tag == tag)
			return pte;
	}
	return NULL;
}

static inline unsigned int
shadow_pte_to_bucket_index(struct proc_info *pi, struct lego_mem_pte *pte)
{
	unsigned long index;

	/* Get PTE offset */
	index = pte - (struct lego_mem_pte *)(pi->soc_shadow_pgtable);

	/* Then to bucket offset */
	index /= FPGA_NUM_PTE_PER_BUCKET;
	return (u32)index;
}

bool check_and_insert_shadow_conflicts(struct proc_info *pi,
				       struct vregion_info *vi,
				       unsigned long addr,
				       unsigned long page_size,
				       unsigned long page_size_shift,
				       unsigned long len);

#endif /* _PGTABLE_H_ */
