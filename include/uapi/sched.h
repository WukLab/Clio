/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _LEGOMEM_UAPI_SCHED_H_
#define _LEGOMEM_UAPI_SCHED_H_

#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/rbtree.h>
#include <pthread.h>
#include <stdatomic.h>

/*
 * The maximum number of processes from a host node
 * allowed to establish connections with a single board.
 *
 * This parameter is also used by hash calculation.
 */
#define NR_MAX_PROCS_PER_NODE	(256)

/*
 * The maximum PID space in a distributed legomem system.
 */
#define NR_MAX_PID		(65535)

typedef atomic_int atomic_t;

#define BOARD_NAME_LEN		(32)
#define PROC_NAME_LEN		(32)

struct vm_area_struct;

struct board_info {
	char			name[BOARD_NAME_LEN];
	unsigned int		board_ip;

	struct list_head	list;

	unsigned long		mem_total;
	unsigned long		mem_avail;
};

/*
 * Notes
 * - Address allocation has 2 fashions:
 *   1) Top Down, 2) Bottom Up
 *   By default, topdown is used.
 *   For a given vRegion, we should avoid doing two fashions
 *   at the same time. There is no guarantee the allocation
 *   will succeed, futher it may tweak the tree in a way
 *   that no allocation will be possible further more.
 */
struct vregion_info {
	unsigned int		flags;
	unsigned long		board_id;

	/*
	 * List of VMAs
	 * Updated by vma_link_list
	 */
	struct vm_area_struct	*mmap;

	/*
	 * RB tree
	 * Updated by vma_link_rb
	 */
	struct rb_root		mm_rb;

	int			nr_vmas;
	unsigned long		highest_vm_end;

	pthread_spinlock_t	lock;
};

/*
 * HACK: If you change anything, remember to check
 * if it should be added to init_proc_info().
 */
struct proc_info {
	unsigned long		flags;

	/*
	 * For hashtable usage
	 * pid and host node id uniquely identify a proc
	 */
        struct hlist_node	link;
	unsigned int		pid;
	unsigned int		node;

	pthread_spinlock_t	lock;
	atomic_t		refcount;

	/* For debugging purpose */
	unsigned int		host_ip;
	char			proc_name[PROC_NAME_LEN];

	struct vregion_info	vregion[NR_VREGIONS];
	int			nr_vmas;
};

#define PROC_INFO_FLAGS_ALLOCATED	0x1

struct vm_unmapped_area_info {
#define VM_UNMAPPED_AREA_TOPDOWN 1
	unsigned long flags;
	unsigned long length;
	unsigned long low_limit;
	unsigned long high_limit;
};

struct vm_area_struct {
	/* First cacheline has info for VMA tree walking */
	unsigned long vm_start;
	unsigned long vm_end; /* The first byte after our end address */
	struct vm_area_struct *vm_next, *vm_prev;
	struct rb_node vm_rb;

	/*
	 * Largest free memory gap in bytes to the left of this VMA.
	 * Either between this VMA and vma->vm_prev, or between one of the
	 * VMAs below us in the VMA rbtree and its ->vm_prev. This helps
	 * get_unmapped_area find a free area of the right size.
	 */
	unsigned long rb_subtree_gap;

	/* Second cache line */
	struct vregion_info *vi;
	unsigned long vm_flags;
};

static inline unsigned int
va_to_vregion_index(unsigned long va)
{
	unsigned long idx;
	idx = va >> VREGION_SIZE_SHIFT;
	return idx;
}

static inline struct vregion_info *
va_to_vregion(struct proc_info *p, unsigned long va)
{
	unsigned int idx;
	struct vregion_info *head;
	head = p->vregion;
	idx = va_to_vregion_index(va);
	return head + idx;
}

static inline unsigned int
vregion_to_index(struct proc_info *p, struct vregion_info *v)
{
	struct vregion_info *head = p->vregion;
	unsigned int idx;

	idx = v - head;
	if (unlikely(idx >= NR_VREGIONS)) {
		BUG();
	}
	return idx;
}

/*
 * The first byte in our address
 */
static inline unsigned long
vregion_to_start_va(struct proc_info *p, struct vregion_info *v)
{
	unsigned int idx;

	idx = vregion_to_index(p, v);

	/* Exclude the 0-0x1000 VA range*/
	if (idx == 0)
		return 0x1000;
	return vregion_to_index(p, v) * VREGION_SIZE;
}

/*
 * The first byte after our end address.
 */
static inline unsigned long
vregion_to_end_va(struct proc_info *p, struct vregion_info *v)
{
	return (vregion_to_index(p, v) + 1) * VREGION_SIZE;
}

static inline void get_proc_info(struct proc_info *pi)
{
	/*
	 * fetch_add returns the old value,
	 * it must have >= 1 refcount before this func.
	 */
	if (unlikely(atomic_fetch_add(&pi->refcount, 1) < 1))
		BUG();
}

void free_proc(struct proc_info *pi);
static inline void put_proc_info(struct proc_info *pi)
{
	/*
	 * fetch_sub returns the old value,
	 * thus we are the last user if it was 1.
	 */
	if (atomic_fetch_sub(&pi->refcount, 1) == 1)
		free_proc(pi);
}

#endif /* _LEGOMEM_UAPI_SCHED_H_ */
