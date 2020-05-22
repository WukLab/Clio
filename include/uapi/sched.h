/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _LEGOMEM_UAPI_SCHED_H_
#define _LEGOMEM_UAPI_SCHED_H_

#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/rbtree.h>
#include <uapi/hashtable.h>
#include <uapi/net_session.h>
#include <uapi/opcode.h>
#include <uapi/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

/*
 * The maximum number of processes from a host node
 * allowed to establish connections with a single board.
 *
 * This parameter is also used by hash calculation.
 */
#define NR_MAX_PROCS_PER_NODE	(256)

/*
 * The maximum PID space in a distributed legomem system.
 *   0                       MAX_USER               MAX_SYSTEM
 *   |-------------------------| ---------------------|
 *   ^                         ^                      ^
 *       global unique             per-board
 *       managed by monitor        private pid space
 */
#define NR_MAX_SYSTEM_PID	(65535)
#define NR_MAX_USER_PID		(60000)

typedef atomic_int atomic_t;

struct vm_area_struct;

/*
 * This determines the per-board session hashtable size.
 * This is not a limit on the maximum number of sessions.
 * A larger number means smaller collision rate at expanses of extra memory.
 */
#define NR_HT_BOARD_SESSIONS_BITS	(3)
#define NR_HT_BOARD_SESSIONS		(1 << NR_HT_BOARD_SESSIONS_BITS)

#define BOARD_INFO_FLAGS_BOARD		(0x1) /* represents a legomem board */
#define BOARD_INFO_FLAGS_HOST		(0x2) /* represents a real host */
#define BOARD_INFO_FLAGS_MONITOR	(0x4) /* represents the monitor */
#define BOARD_INFO_FLAGS_DUMMY		(0x8) /* a dummy bi */
#define BOARD_INFO_FLAGS_LOCALHOST	(0x10) /* localhost bi */
#define BOARD_INFO_FLAGS_BITS_MASK	(0x1f)

#define BOARD_INFO_FLAGS_ALLOCATED	(0x20)

struct board_info {
	int			board_id;
	char			name[BOARD_NAME_LEN];
	int			board_ip;
	unsigned int		udp_port;
	unsigned long		flags;

	/*
	 * The endpoint info of this sepcific network session
	 * The Ethernet/IP/UDP header info, 44 bytes
	 */
	struct endpoint_info	local_ei, remote_ei;

	/* List boards together */
	struct list_head	list;
	struct hlist_node	link;

	/*
	 * This is the default mgmt session that all boards
	 * will open and accept requests.
	 */
	struct session_net	*mgmt_session;

	/* The hashtable for open sessions with this board */
	struct hlist_head	ht_sessions[NR_HT_BOARD_SESSIONS];
	pthread_spinlock_t	lock;

	unsigned long		mem_total;
	unsigned long		mem_avail;

	unsigned long		*stat;
} ____cacheline_aligned;

#define ANY_BOARD	(UINT_MAX)	/* return the first real board */
#define ANY_NODE	(UINT_MAX-1)	/* return anything except special BIs */

/*
 * Special board_info types are created by the system, for special usages.
 * They do not have remote counterparts.
 */
static inline bool special_board_info_type(unsigned long type)
{
	type &= BOARD_INFO_FLAGS_BITS_MASK;
	if (type == BOARD_INFO_FLAGS_MONITOR ||
	    type == BOARD_INFO_FLAGS_DUMMY ||
	    type == BOARD_INFO_FLAGS_LOCALHOST)
		return true;
	return false;
}

static inline char *board_info_type_str(unsigned long type)
{
	type &= BOARD_INFO_FLAGS_BITS_MASK;
	switch (type) {
	case BOARD_INFO_FLAGS_BOARD:		return "board";
	case BOARD_INFO_FLAGS_HOST:		return "host";
	case BOARD_INFO_FLAGS_MONITOR:		return "monitor";
	case BOARD_INFO_FLAGS_DUMMY:		return "dummy";
	case BOARD_INFO_FLAGS_LOCALHOST:	return "localhost";
	default:				return "unknown";
	}
	return "error";
}

static inline struct session_net *
get_board_mgmt_session(struct board_info *bi)
{
	return bi->mgmt_session;
}

static inline void
set_board_mgmt_session(struct board_info *bi, struct session_net *ses)
{
	bi->mgmt_session = ses;
}

static inline int init_board_info(struct board_info *bi)
{
	INIT_LIST_HEAD(&bi->list);

	hash_init(bi->ht_sessions);
	pthread_spin_init(&bi->lock, PTHREAD_PROCESS_PRIVATE);

	bi->stat = malloc(NR_STAT_TYPES * sizeof(unsigned long));
	if (!bi->stat)
		return -ENOMEM;
	memset(bi->stat, 0, NR_STAT_TYPES * sizeof(unsigned long));
	return 0;
}

static inline int
board_add_session(struct board_info *p, struct session_net *ses)
{
	int key;

	key = ses->session_id;

	pthread_spin_lock(&p->lock);
	hash_add(p->ht_sessions, &ses->ht_link_board, key);
	pthread_spin_unlock(&p->lock);

	return 0;
}

static inline int
board_remove_session(struct board_info *p, struct session_net *ses)
{
	struct session_net *_ses;
	int key;

	key = ses->session_id;

	pthread_spin_lock(&p->lock);
	hash_for_each_possible(p->ht_sessions, _ses, ht_link_board, key) {
		if (likely(_ses->session_id == ses->session_id)) {
			hash_del(&ses->ht_link_board);
			pthread_spin_unlock(&p->lock);
			return 0;
		}
	}
	pthread_spin_unlock(&p->lock);

	return -1;
}

static inline struct session_net *
board_find_session(struct board_info *p, int session_id)
{
	struct session_net *ses;
	int key;

	key = session_id;

	pthread_spin_lock(&p->lock);
	hash_for_each_possible(p->ht_sessions, ses, ht_link_board, key) {
		if (likely(ses->session_id == session_id)) {
			pthread_spin_unlock(&p->lock);
			return ses;
		}
	}
	pthread_spin_unlock(&p->lock);
	return NULL;
}

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
#define VREGION_INFO_FLAG_ALLOCATED		(0x1)
#define VREGION_INFO_FLAG_UNMAPPED_AREA_TOPDOWN	(0x2)
struct vregion_info {
	unsigned int		flags;
	int			board_ip;
	unsigned int		udp_port;

	struct list_head	list;

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
	 * Used by SoC code only
	 * fpga_pgtable points to the real pgtbale in FPGA
	 * soc_shadow_pgtable points the shadow copy in DRAM
	 */
	void 			*fpga_pgtable;
	void			*soc_shadow_pgtable;

	/*
	 * For hashtable usage
	 * pid and host node id uniquely identify a proc
	 */
        struct hlist_node	link;
	unsigned int		pid;
	unsigned int		node;

	pthread_spinlock_t	lock;
	atomic_t		refcount;

	struct vregion_info	vregion[NR_VREGIONS];
	struct list_head	free_list_head;
	int			nr_vmas;

	/*
	 * Only used by SoC's handle_ctrl_alloc
	 * which is self-managing vRegion array without monitor.
	 */
	int			cached_vregion_index;

	/* For debugging purpose */
	unsigned int		host_ip;
	char			host_ip_str[INET_ADDRSTRLEN];
	char			proc_name[PROC_NAME_LEN];
} ____cacheline_aligned;

static inline struct vregion_info * 
__vregion_freelist_dequeue_head(struct proc_info *ctx)
{
	struct vregion_info *v;

	v = list_entry(ctx->free_list_head.next, struct vregion_info, list);
	list_del(&v->list);
	return v;
}

static inline struct vregion_info * 
vregion_freelist_dequeue_head(struct proc_info *ctx)
{
	struct vregion_info *v = NULL;

	pthread_spin_lock(&ctx->lock);
	if (!list_empty(&ctx->free_list_head))
		v = __vregion_freelist_dequeue_head(ctx);
	pthread_spin_unlock(&ctx->lock);
	return v;
}

static inline void
__vregion_freelist_enqueue_tail(struct proc_info *ctx,
			      struct vregion_info *v)
{
	list_add_tail(&v->list, &ctx->free_list_head);
}

static inline void
vregion_freelist_enqueue_tail(struct proc_info *ctx,
			      struct vregion_info *v)
{
	pthread_spin_lock(&ctx->lock);
	__vregion_freelist_enqueue_tail(ctx, v);
	pthread_spin_unlock(&ctx->lock);
}

static inline void init_vregion(struct vregion_info *v)
{
	v->flags = VREGION_INFO_FLAG_UNMAPPED_AREA_TOPDOWN;
	v->mmap = NULL;
	v->mm_rb = RB_ROOT;
	v->nr_vmas = 0;
	v->highest_vm_end = 0;
	pthread_spin_init(&v->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void init_proc_info(struct proc_info *pi)
{
	int j;
	struct vregion_info *v;

	pi->flags = 0;

	INIT_HLIST_NODE(&pi->link);
	pi->pid = 0;
	pi->node = 0;

	pthread_spin_init(&pi->lock, PTHREAD_PROCESS_PRIVATE);
	atomic_init(&pi->refcount, 1);

	/* Vregion Related */
	INIT_LIST_HEAD(&pi->free_list_head);
	pi->nr_vmas = 0;
	for (j = 0; j < NR_VREGIONS; j++) {
		v = pi->vregion + j;
		init_vregion(v);

		if (j != 0)
			__vregion_freelist_enqueue_tail(pi, v);
	}
}

struct vm_unmapped_area_info {
	/*
	 * Use VREGION_INFO flags
	 * VREGION_INFO_FLAG_UNMAPPED_AREA_TOPDOWN
	 */
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

	/*
	 * Used by shadow pgtables
	 * to chain all the conflicting VMAs..
	 */
	struct list_head conflict_list;
};

static inline unsigned long __remote
vregion_index_to_va(unsigned int idx)
{
	return (unsigned long)idx << VREGION_SIZE_SHIFT;
}

static inline unsigned int
va_to_vregion_index(unsigned long __remote va)
{
	return (unsigned int)(va >> VREGION_SIZE_SHIFT);
}

static inline struct vregion_info *
va_to_vregion(struct proc_info *p, unsigned long __remote va)
{
	unsigned int idx;
	struct vregion_info *head;
	head = p->vregion;
	idx = va_to_vregion_index(va);
	return head + idx;
}

static inline struct vregion_info *
index_to_vregion(struct proc_info *p, unsigned int index)
{
	BUG_ON(index >=  NR_VREGIONS);
	return p->vregion + index;
}

static inline unsigned long
vregion_to_index(struct proc_info *p, struct vregion_info *v)
{
	struct vregion_info *head = p->vregion;
	unsigned long idx;

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
	unsigned long idx;

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
