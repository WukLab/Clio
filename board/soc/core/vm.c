/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * This file describes per-vRegion VMA tree.
 * We only have a few public APIs: alloc and free.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/rbtree.h>
#include <uapi/compiler.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

#include "core.h"
#include "pgtable.h"

#define VM_WARN_ON(cond)                                                       \
	do {                                                                   \
	} while (0)

static inline unsigned long vma_compute_gap(struct vm_area_struct *vma)
{
	unsigned long gap, prev_end;

	gap = vma->vm_start;
	if (vma->vm_prev) {
		prev_end = vma->vm_prev->vm_end;
		if (gap > prev_end)
			gap -= prev_end;
		else
			gap = 0;
	}
	return gap;
}

#define CONFIG_DEBUG_VM_RB
#ifdef CONFIG_DEBUG_VM_RB
static unsigned long vma_compute_subtree_gap(struct vm_area_struct *vma)
{
	unsigned long max = vma_compute_gap(vma), subtree_gap;
	if (vma->vm_rb.rb_left) {
		subtree_gap = rb_entry(vma->vm_rb.rb_left,
				       struct vm_area_struct, vm_rb)
				      ->rb_subtree_gap;
		if (subtree_gap > max)
			max = subtree_gap;
	}
	if (vma->vm_rb.rb_right) {
		subtree_gap = rb_entry(vma->vm_rb.rb_right,
				       struct vm_area_struct, vm_rb)
				      ->rb_subtree_gap;
		if (subtree_gap > max)
			max = subtree_gap;
	}
	return max;
}

static int browse_rb(struct vregion_info *vi)
{
	struct rb_root *root = &vi->mm_rb;
	int i = 0, j, bug = 0;
	struct rb_node *nd, *pn = NULL;
	unsigned long prev = 0, pend = 0;

	for (nd = rb_first(root); nd; nd = rb_next(nd)) {
		struct vm_area_struct *vma;
		vma = rb_entry(nd, struct vm_area_struct, vm_rb);
		if (vma->vm_start < prev) {
			printf("vm_start %lx < prev %lx\n", vma->vm_start,
			       prev);
			bug = 1;
		}
		if (vma->vm_start < pend) {
			printf("vm_start %lx < pend %lx\n", vma->vm_start,
			       pend);
			bug = 1;
		}
		if (vma->vm_start > vma->vm_end) {
			printf("vm_start %lx > vm_end %lx\n", vma->vm_start,
			       vma->vm_end);
			bug = 1;
		}

		if (vma->rb_subtree_gap != vma_compute_subtree_gap(vma)) {
			printf("free gap %lx, correct %lx\n",
			       vma->rb_subtree_gap,
			       vma_compute_subtree_gap(vma));
			bug = 1;
		}

		i++;
		pn = nd;
		prev = vma->vm_start;
		pend = vma->vm_end;
	}

	j = 0;
	for (nd = pn; nd; nd = rb_prev(nd))
		j++;
	if (i != j) {
		printf("backwards %d, forwards %d\n", j, i);
		bug = 1;
	}
	return bug ? -1 : i;
}

static void validate_mm_rb(struct rb_root *root, struct vm_area_struct *ignore)
{
	struct rb_node *nd;

	for (nd = rb_first(root); nd; nd = rb_next(nd)) {
		struct vm_area_struct *vma;
		vma = rb_entry(nd, struct vm_area_struct, vm_rb);
		if (vma != ignore &&
		    vma->rb_subtree_gap != vma_compute_subtree_gap(vma))
			BUG();
	}
}

static void validate_vregion(struct vregion_info *vi)
{
	int bug = 0;
	int i = 0;
	unsigned long highest_address = 0;
	struct vm_area_struct *vma = vi->mmap;

	while (vma) {
		highest_address = vma->vm_end;
		vma = vma->vm_next;
		i++;
	}
	if (i != vi->nr_vmas) {
		printf("nr_vmas %d vm_next %d\n", vi->nr_vmas, i);
		bug = 1;
	}
	if (highest_address != vi->highest_vm_end) {
		printf("vi->highest_vm_end %lx, found %lx\n",
		       vi->highest_vm_end, highest_address);
		bug = 1;
	}
	i = browse_rb(vi);
	if (i != vi->nr_vmas) {
		if (i != -1)
			printf("nr_vmas %d rb %d\n", vi->nr_vmas, i);
		bug = 1;
	}
	BUG_ON(bug);
}
#else
#define validate_mm_rb(root, ignore)                                           \
	do {                                                                   \
	} while (0)
#define validate_vregion(vi)                                                   \
	do {                                                                   \
	} while (0)
#endif

static inline struct vm_area_struct *vm_area_alloc(void)
{
	struct vm_area_struct *vma;

	vma = malloc(sizeof(*vma));
	if (vma) {
		memset(vma, 0, sizeof(*vma));
	}
	return vma;
}

static inline struct vm_area_struct *vm_area_dup(struct vm_area_struct *orig)
{
	struct vm_area_struct *new;

	new = malloc(sizeof(*new));
	if (new) {
		*new = *orig;
	}
	return new;
}

static inline void vm_area_free(struct vm_area_struct *vma)
{
	free(vma);
}

static void __vma_link_list(struct vregion_info *vi, struct vm_area_struct *vma,
			    struct vm_area_struct *prev)
{
	struct vm_area_struct *next;

	vma->vm_prev = prev;
	if (prev) {
		next = prev->vm_next;
		prev->vm_next = vma;
	} else {
		next = vi->mmap;
		vi->mmap = vma;
	}
	vma->vm_next = next;
	if (next)
		next->vm_prev = vma;
}

static void __vma_unlink_list(struct vregion_info *vi,
			      struct vm_area_struct *vma)
{
	struct vm_area_struct *prev, *next;

	next = vma->vm_next;
	prev = vma->vm_prev;
	if (prev)
		prev->vm_next = next;
	else
		vi->mmap = next;
	if (next)
		next->vm_prev = prev;
}

RB_DECLARE_CALLBACKS_MAX(static, vma_gap_callbacks, struct vm_area_struct,
			 vm_rb, unsigned long, rb_subtree_gap, vma_compute_gap)

/*
 * Update augmented rbtree rb_subtree_gap values after vma->vm_start or
 * vma->vm_prev->vm_end values changed, without modifying the vma's position
 * in the rbtree.
 */
static void vma_gap_update(struct vm_area_struct *vma)
{
	/*
	 * As it turns out, RB_DECLARE_CALLBACKS_MAX() already created
	 * a callback function that does exactly what we want.
	 */
	vma_gap_callbacks_propagate(&vma->vm_rb, NULL);
}

static inline void vma_rb_insert(struct vm_area_struct *vma,
				 struct rb_root *root)
{
	/* All rb_subtree_gap values must be consistent prior to insertion */
	validate_mm_rb(root, NULL);

	rb_insert_augmented(&vma->vm_rb, root, &vma_gap_callbacks);
}

static void __vma_rb_erase(struct vm_area_struct *vma, struct rb_root *root)
{
	/*
	 * Note rb_erase_augmented is a fairly large inline function,
	 * so make sure we instantiate it only once with our desired
	 * augmented rbtree callbacks.
	 */
	rb_erase_augmented(&vma->vm_rb, root, &vma_gap_callbacks);
}

static __always_inline void vma_rb_erase_ignore(struct vm_area_struct *vma,
						struct rb_root *root,
						struct vm_area_struct *ignore)
{
	/*
	 * All rb_subtree_gap values must be consistent prior to erase,
	 * with the possible exception of the "next" vma being erased if
	 * next->vm_start was reduced.
	 */
	validate_mm_rb(root, ignore);

	__vma_rb_erase(vma, root);
}

static __always_inline void vma_rb_erase(struct vm_area_struct *vma,
					 struct rb_root *root)
{
	/*
	 * All rb_subtree_gap values must be consistent prior to erase,
	 * with the possible exception of the vma being erased.
	 */
	validate_mm_rb(root, vma);

	__vma_rb_erase(vma, root);
}

void __vma_link_rb(struct vregion_info *vi, struct vm_area_struct *vma,
		   struct rb_node **rb_link, struct rb_node *rb_parent)
{
	/* Update tracking information for the gap following the new vma. */
	if (vma->vm_next)
		vma_gap_update(vma->vm_next);
	else
		vi->highest_vm_end = vma->vm_end;

	/*
	 * vma->vm_prev wasn't known when we followed the rbtree to find the
	 * correct insertion point for that vma. As a result, we could not
	 * update the vma vm_rb parents rb_subtree_gap values on the way down.
	 * So, we first insert the vma with a zero rb_subtree_gap value
	 * (to be consistent with what we did on the way down), and then
	 * immediately update the gap to the correct value. Finally we
	 * rebalance the rbtree after all augmented values have been set.
	 */
	rb_link_node(&vma->vm_rb, rb_parent, rb_link);
	vma->rb_subtree_gap = 0;
	vma_gap_update(vma);
	vma_rb_insert(vma, &vi->mm_rb);
}

/*
 * Insert a newly created @vma into list and RB tree.
 * They are vi->vma and vi->mm_rb, respectively.
 */
static inline void __vma_link(struct vregion_info *vi,
			      struct vm_area_struct *vma,
			      struct vm_area_struct *prev,
			      struct rb_node **rb_link,
			      struct rb_node *rb_parent)
{
	__vma_link_list(vi, vma, prev);
	__vma_link_rb(vi, vma, rb_link, rb_parent);
}

static inline void vma_link(struct vregion_info *vi, struct vm_area_struct *vma,
			    struct vm_area_struct *prev,
			    struct rb_node **rb_link, struct rb_node *rb_parent)
{
	__vma_link(vi, vma, prev, rb_link, rb_parent);
	vi->nr_vmas++;
	validate_vregion(vi);
}

/*
 * Find if [addr, end) already exist in the RB tree.
 */
static int find_vma_links(struct vregion_info *vi, unsigned long addr,
			  unsigned long end, struct vm_area_struct **pprev,
			  struct rb_node ***rb_link, struct rb_node **rb_parent)
{
	struct rb_node **__rb_link, *__rb_parent, *rb_prev;

	__rb_link = &vi->mm_rb.rb_node;
	rb_prev = __rb_parent = NULL;

	while (*__rb_link) {
		struct vm_area_struct *vma_tmp;

		__rb_parent = *__rb_link;
		vma_tmp = rb_entry(__rb_parent, struct vm_area_struct, vm_rb);

		if (vma_tmp->vm_end > addr) {
			/* Fail if an existing vma overlaps the area */
			if (vma_tmp->vm_start < end)
				return -ENOMEM;
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	*pprev = NULL;
	if (rb_prev)
		*pprev = rb_entry(rb_prev, struct vm_area_struct, vm_rb);
	*rb_link = __rb_link;
	*rb_parent = __rb_parent;
	return 0;
}

/* Look up the first VMA which satisfies  addr < vm_end,  NULL if none. */
struct vm_area_struct *find_vma(struct vregion_info *vi, unsigned long addr)
{
	struct rb_node *rb_node = NULL;
	struct vm_area_struct *vma = NULL;

	rb_node = vi->mm_rb.rb_node;

	while (rb_node) {
		struct vm_area_struct *tmp;

		tmp = rb_entry(rb_node, struct vm_area_struct, vm_rb);

		if (tmp->vm_end > addr) {
			vma = tmp;
			if (tmp->vm_start <= addr)
				break;
			rb_node = rb_node->rb_left;
		} else
			rb_node = rb_node->rb_right;
	}
	return vma;
}

/*
 * Helper for vma_adjust() in the split_vma insert case: insert a vma into the
 * mm's list and rbtree.  It has already been inserted into the interval tree.
 */
static void __insert_vm_struct(struct vregion_info *vi,
			       struct vm_area_struct *vma)
{
	struct vm_area_struct *prev;
	struct rb_node **rb_link, *rb_parent;

	if (find_vma_links(vi, vma->vm_start, vma->vm_end, &prev, &rb_link,
			   &rb_parent))
		BUG();
	__vma_link(vi, vma, prev, rb_link, rb_parent);
	vi->nr_vmas++;
}

static __always_inline void __vma_unlink_common(struct vregion_info *vi,
						struct vm_area_struct *vma,
						struct vm_area_struct *ignore)
{
	vma_rb_erase_ignore(vma, &vi->mm_rb, ignore);
	__vma_unlink_list(vi, vma);
}

/*
 * We cannot adjust vm_start, vm_end, vm_pgoff fields of a vma that
 * is already present in an i_mmap tree without adjusting the tree.
 * The following helper function should be used when such adjustments
 * are necessary.  The "insert" vma (if any) is to be inserted
 * before we drop the necessary locks.
 */
int __vma_adjust(struct vregion_info *vi, struct vm_area_struct *vma,
		 unsigned long start, unsigned long end,
		 struct vm_area_struct *insert, struct vm_area_struct *expand)
{
	struct vm_area_struct *next = vma->vm_next;
	bool start_changed = false, end_changed = false;
	long adjust_next = 0;
	int remove_next = 0;

	if (next && !insert) {
		if (end >= next->vm_end) {
			/*
			 * vma expands, overlapping all the next, and
			 * perhaps the one after too (mprotect case 6).
			 * The only other cases that gets here are
			 * case 1, case 7 and case 8.
			 */
			if (next == expand) {
				/*
				 * The only case where we don't expand "vma"
				 * and we expand "next" instead is case 8.
				 */
				VM_WARN_ON(end != next->vm_end);

				/*
				 * remove_next == 3 means we're
				 * removing "vma" and that to do so we
				 * swapped "vma" and "next".
				 */
				remove_next = 3;
				VM_WARN_ON(file != next->vm_file);
				swap(vma, next);
			} else {
				VM_WARN_ON(expand != vma);
				/*
				 * case 1, 6, 7, remove_next == 2 is case 6,
				 * remove_next == 1 is case 1 or 7.
				 */
				remove_next = 1 + (end > next->vm_end);
				VM_WARN_ON(remove_next == 2 &&
					   end != next->vm_next->vm_end);
				/* trim end to next, for case 6 first pass */
				end = next->vm_end;
			}
		} else if (end > next->vm_start) {
			/*
			 * vma expands, overlapping part of the next:
			 * mprotect case 5 shifting the boundary up.
			 */
			adjust_next = (end - next->vm_start);
		} else if (end < vma->vm_end) {
			/*
			 * vma shrinks, and !insert tells it's not
			 * split_vma inserting another: so it must be
			 * mprotect case 4 shifting the boundary down.
			 */
			adjust_next = -((vma->vm_end - end));
		}
	}
again:
	if (start != vma->vm_start) {
		vma->vm_start = start;
		start_changed = true;
	}
	if (end != vma->vm_end) {
		vma->vm_end = end;
		end_changed = true;
	}
	if (adjust_next) {
		next->vm_start += adjust_next;
	}

	if (remove_next) {
		/*
		 * vma_merge has merged next into vma, and needs
		 * us to remove next before dropping the locks.
		 */
		if (remove_next != 3)
			__vma_unlink_common(vi, next, next);
		else
			/*
			 * vma is not before next if they've been
			 * swapped.
			 *
			 * pre-swap() next->vm_start was reduced so
			 * tell validate_mm_rb to ignore pre-swap()
			 * "next" (which is stored in post-swap()
			 * "vma").
			 */
			__vma_unlink_common(vi, next, vma);
	} else if (insert) {
		/*
		 * split_vma has split insert from vma, and needs
		 * us to insert it before dropping the locks
		 * (it may either follow vma or precede it).
		 */
		__insert_vm_struct(vi, insert);
	} else {
		if (start_changed)
			vma_gap_update(vma);
		if (end_changed) {
			if (!next)
				vi->highest_vm_end = vma->vm_end;
			else if (!adjust_next)
				vma_gap_update(next);
		}
	}

	if (remove_next) {
		vi->nr_vmas--;
		vm_area_free(next);

		/*
		 * In mprotect's case 6 (see comments on vma_merge),
		 * we must remove another next too. It would clutter
		 * up the code too much to do both in one go.
		 */
		if (remove_next != 3) {
			/*
			 * If "next" was removed and vma->vm_end was
			 * expanded (up) over it, in turn
			 * "next->vm_prev->vm_end" changed and the
			 * "vma->vm_next" gap must be updated.
			 */
			next = vma->vm_next;
		} else {
			/*
			 * For the scope of the comment "next" and
			 * "vma" considered pre-swap(): if "vma" was
			 * removed, next->vm_start was expanded (down)
			 * over it and the "next" gap must be updated.
			 * Because of the swap() the post-swap() "vma"
			 * actually points to pre-swap() "next"
			 * (post-swap() "next" as opposed is now a
			 * dangling pointer).
			 */
			next = vma;
		}
		if (remove_next == 2) {
			remove_next = 1;
			end = next->vm_end;
			goto again;
		} else if (next)
			vma_gap_update(next);
		else {
			/*
			 * If remove_next == 2 we obviously can't
			 * reach this path.
			 *
			 * If remove_next == 3 we can't reach this
			 * path because pre-swap() next is always not
			 * NULL. pre-swap() "next" is not being
			 * removed and its next->vm_end is not altered
			 * (and furthermore "end" already matches
			 * next->vm_end in remove_next == 3).
			 *
			 * We reach this only in the remove_next == 1
			 * case if the "next" vma that was removed was
			 * the highest vma of the mm. However in such
			 * case next->vm_end == "end" and the extended
			 * "vma" has vma->vm_end == next->vm_end so
			 * vi->highest_vm_end doesn't need any update
			 * in remove_next == 1 case.
			 */
			VM_WARN_ON(vi->highest_vm_end != vma->vm_end);
		}
	}

	validate_vregion(vi);
	return 0;
}

static inline int vma_adjust(struct vregion_info *vi,
			     struct vm_area_struct *vma, unsigned long start,
			     unsigned long end, struct vm_area_struct *insert)
{
	return __vma_adjust(vi, vma, start, end, insert, NULL);
}

/*
 * Split a vma into two pieces at address 'addr', a new vma is allocated
 * either for the first part or the tail.
 */
int split_vma(struct vregion_info *vi, struct vm_area_struct *vma,
	      unsigned long addr, int new_below)
{
	struct vm_area_struct *new;
	int err;

	new = vm_area_dup(vma);
	if (!new)
		return -ENOMEM;

	if (new_below)
		new->vm_end = addr;
	else
		new->vm_start = addr;

	if (new_below)
		err = vma_adjust(vi, vma, addr, vma->vm_end, new);
	else
		err = vma_adjust(vi, vma, vma->vm_start, addr, new);

	if (unlikely(err))
		vm_area_free(new);
	return err;
}

/*
 * TODO: we should check permissions at least!
 */
static int can_vma_merge(struct vm_area_struct *vma, unsigned long vm_flags)
{
	return 1;
}

/*
 * Given a mapping request (addr,end,vm_flags,file,pgoff), figure out
 * whether that can be merged with its predecessor or its successor.
 * Or both (it neatly fills a hole).
 *
 * In most cases - when called for mmap, brk or mremap - [addr,end) is
 * certain not to be mapped by the time vma_merge is called; but when
 * called for mprotect, it is certain to be already mapped (either at
 * an offset within prev, or at the start of next), and the flags of
 * this area are about to be changed to vm_flags - and the no-change
 * case has already been eliminated.
 *
 * The following mprotect cases have to be considered, where AAAA is
 * the area passed down from mprotect_fixup, never extending beyond one
 * vma, PPPPPP is the prev vma specified, and NNNNNN the next vma after:
 *
 *     AAAA             AAAA                   AAAA
 *    PPPPPPNNNNNN    PPPPPPNNNNNN       PPPPPPNNNNNN
 *    cannot merge    might become       might become
 *                    PPNNNNNNNNNN       PPPPPPPPPPNN
 *    mmap, brk or    case 4 below       case 5 below
 *    mremap move:
 *                        AAAA               AAAA
 *                    PPPP    NNNN       PPPPNNNNXXXX
 *                    might become       might become
 *                    PPPPPPPPPPPP 1 or  PPPPPPPPPPPP 6 or
 *                    PPPPPPPPNNNN 2 or  PPPPPPPPXXXX 7 or
 *                    PPPPNNNNNNNN 3     PPPPXXXXXXXX 8
 *
 * It is important for case 8 that the vma NNNN overlapping the
 * region AAAA is never going to extended over XXXX. Instead XXXX must
 * be extended in region AAAA and NNNN must be removed. This way in
 * all cases where vma_merge succeeds, the moment vma_adjust drops the
 * rmap_locks, the properties of the merged vma will be already
 * correct for the whole merged range. Some of those properties like
 * vm_page_prot/vm_flags may be accessed by rmap_walks and they must
 * be correct for the whole merged range immediately after the
 * rmap_locks are released. Otherwise if XXXX would be removed and
 * NNNN would be extended over the XXXX range, remove_migration_ptes
 * or other rmap walkers (if working on addresses beyond the "end"
 * parameter) may establish ptes with the wrong permissions of NNNN
 * instead of the right permissions of XXXX.
 */
struct vm_area_struct *vma_merge(struct vregion_info *vi,
				 struct vm_area_struct *prev,
				 unsigned long addr, unsigned long end,
				 unsigned long vm_flags)
{
	struct vm_area_struct *area, *next;
	int err;

	if (prev)
		next = prev->vm_next;
	else
		next = vi->mmap;
	area = next;
	if (area && area->vm_end == end) /* cases 6, 7, 8 */
		next = next->vm_next;

	/* verify some invariant that must be enforced by the caller */
	VM_WARN_ON(prev && addr <= prev->vm_start);
	VM_WARN_ON(area && end > area->vm_end);
	VM_WARN_ON(addr >= end);

	/*
	 * Can it merge with the predecessor?
	 */
	if (prev && prev->vm_end == addr && can_vma_merge(prev, vm_flags)) {
		/*
		 * OK, it can.  Can we now merge in the successor as well?
		 */
		if (next && end == next->vm_start &&
		    can_vma_merge(next, vm_flags)) {
			/* cases 1, 6 */
			err = __vma_adjust(vi, prev, prev->vm_start,
					   next->vm_end, NULL, prev);
		} else /* cases 2, 5, 7 */
			err = __vma_adjust(vi, prev, prev->vm_start, end, NULL,
					   prev);
		if (err)
			return NULL;
		return prev;
	}

	/*
	 * Can this new request be merged in front of next?
	 */
	if (next && end == next->vm_start && can_vma_merge(next, vm_flags)) {
		if (prev && addr < prev->vm_end) /* case 4 */
			err = __vma_adjust(vi, prev, prev->vm_start, addr, NULL,
					   next);
		else { /* cases 3, 8 */
			err = __vma_adjust(vi, area, addr, next->vm_end, NULL,
					   next);
			/*
			 * In case 3 area is already equal to next and
			 * this is a noop, but in case 8 "area" has
			 * been removed and next was expanded over it.
			 */
			area = next;
		}
		if (err)
			return NULL;
		return area;
	}

	return NULL;
}

static void unmap_single_vma(struct proc_info *proc, struct vm_area_struct *vma,
			     unsigned long start_addr, unsigned long end_addr)
{
	unsigned long start = max(vma->vm_start, start_addr);
	unsigned long end;

	if (start >= vma->vm_end)
		return;

	end = min(vma->vm_end, end_addr);
	if (end <= vma->vm_start)
		return;

	/*
	 * CONFLICT VMAs are not real VMAs mapped to pgtables.
	 * They are created during __alloc_va.
	 * They should not call into pgtable code now.
	 */
	if ((start != end) && (vma->vm_flags != LEGOMEM_VM_FLAGS_CONFLICT))
		free_fpga_pte_range(proc, start, end, PAGE_SIZE);
}

static void unmap_region(struct proc_info *proc,
			 struct vregion_info *vi, struct vm_area_struct *vma,
			 unsigned long start, unsigned long end)
{
	for (; vma && vma->vm_start < end; vma = vma->vm_next)
		unmap_single_vma(proc, vma, start, end);
}

/*
 * Create a list of vma's touched by the unmap/free,
 * removing them from the vRegion's vma list.
 *
 * They are just removed, they are not freed by this func.
 */
static void detach_vmas_to_be_unmapped(struct vregion_info *vi,
				       struct vm_area_struct *vma,
				       struct vm_area_struct *prev,
				       unsigned long end)
{
	struct vm_area_struct **insertion_point;
	struct vm_area_struct *tail_vma = NULL;

	insertion_point = (prev ? &prev->vm_next : &vi->mmap);
	vma->vm_prev = NULL;

	do {
		vma_rb_erase(vma, &vi->mm_rb);
		vi->nr_vmas--;
		tail_vma = vma;
		vma = vma->vm_next;
	} while (vma && vma->vm_start < end);

	*insertion_point = vma;
	if (vma) {
		vma->vm_prev = prev;
		vma_gap_update(vma);
	} else
		vi->highest_vm_end = prev ? prev->vm_end : 0;
	tail_vma->vm_next = NULL;
}

/*
 * Lock is already held upon entry.
 */
int __free_va(struct proc_info *proc, struct vregion_info *vi,
	      unsigned long start, unsigned long len)
{
	unsigned long end;
	struct vm_area_struct *vma, *prev, *last;

	if (len == 0)
		return -EINVAL;
	end = start + len;

	/* Find the first overlapping VMA */
	vma = find_vma(vi, start);
	if (!vma)
		return 0;

	prev = vma->vm_prev;
	/* we have  start < vma->vm_end  */

	/* if it doesn't overlap, we have nothing.. */
	if (vma->vm_start >= end)
		return 0;

	/*
	 * If we need to split any vma, do it now to save pain later.
	 */
	if (start > vma->vm_start) {
		int error;

		/*
		 * Make sure that map_count on return from munmap() will
		 * not exceed its limit; but let map_count go just above
		 * its limit temporarily, to help free resources as expected.
		 */
		if (end < vma->vm_end)
			return -ENOMEM;

		error = split_vma(vi, vma, start, 0);
		if (error)
			return error;
		prev = vma;
	}

	/* Does it split the last one? */
	last = find_vma(vi, end);
	if (last && end > last->vm_start) {
		int error = split_vma(vi, last, end, 1);
		if (error)
			return error;
	}
	vma = prev ? prev->vm_next : vi->mmap;

	/*
	 * Detach VMAs from RBtree
	 */
	detach_vmas_to_be_unmapped(vi, vma, prev, end);

	/*
	 * Notify FPGA side to invalidate
	 * TLB and free PTE entries..
	 */
	unmap_region(proc, vi, vma, start, end);

	/*
	 * Last step, free the VMAs
	 * Now they are off the record
	 */
	do {
		struct vm_area_struct *next = vma->vm_next;
		vm_area_free(vma);
		vma = next;
	} while (vma);
	validate_vregion(vi);

	return 0;
}

unsigned long vma_tree_new(struct proc_info *proc, struct vregion_info *vi,
			   unsigned long addr, unsigned long len,
			   unsigned long vm_flags)
{
	struct vm_area_struct *vma, *prev;
	struct rb_node **rb_link, *rb_parent;

	/* Clear old maps */
	while (find_vma_links(vi, addr, addr + len, &prev, &rb_link,
			      &rb_parent)) {
		if (__free_va(proc, vi, addr, len))
			return -ENOMEM;
	}

	/* Can we just expand an old mapping? */
	vma = vma_merge(vi, prev, addr, addr + len, vm_flags);
	if (vma)
		goto out;

	/* Okay, creat new one and link with current tree */
	vma = vm_area_alloc();
	if (!vma)
		return -ENOMEM;

	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_flags = vm_flags;

	vma_link(vi, vma, prev, rb_link, rb_parent);
out:
	return addr;
}

/*
 * We implement the search by looking for an rbtree node that
 * immediately follows a suitable gap. That is,
 * - gap_start = vma->vm_prev->vm_end <= info->high_limit - length;
 * - gap_end   = vma->vm_start        >= info->low_limit  + length;
 * - gap_end - gap_start >= length
 *
 * Return gap_start.
 */
static unsigned long
__find_va_range_bottomup(struct proc_info *proc, struct vregion_info *vi,
			 struct vm_unmapped_area_info *info)
{
	struct vm_area_struct *vma;
	unsigned long length, low_limit, high_limit, gap_start, gap_end;

	length = info->length;

	/* Adjust search limits by the desired length */
	if (info->high_limit < length)
		return -ENOMEM;
	high_limit = info->high_limit - length;

	if (info->low_limit > high_limit)
		return -ENOMEM;
	low_limit = info->low_limit + length;

	/* Check if rbtree root looks promising */
	if (RB_EMPTY_ROOT(&vi->mm_rb))
		goto check_highest;

	vma = rb_entry(vi->mm_rb.rb_node, struct vm_area_struct, vm_rb);
	if (vma->rb_subtree_gap < length)
		goto check_highest;

	while (true) {
		/* Visit left subtree if it looks promising */
		gap_end = vma->vm_start;
		if (gap_end >= low_limit && vma->vm_rb.rb_left) {
			struct vm_area_struct *left =
				rb_entry(vma->vm_rb.rb_left,
					 struct vm_area_struct, vm_rb);
			if (left->rb_subtree_gap >= length) {
				vma = left;
				continue;
			}
		}

		gap_start = vma->vm_prev ? vma->vm_prev->vm_end : 0;
	check_current:
		/* Check if current node has a suitable gap */
		if (gap_start > high_limit)
			return -ENOMEM;
		if (gap_end >= low_limit && gap_end - gap_start >= length)
			goto found;

		/* Visit right subtree if it looks promising */
		if (vma->vm_rb.rb_right) {
			struct vm_area_struct *right =
				rb_entry(vma->vm_rb.rb_right,
					 struct vm_area_struct, vm_rb);
			if (right->rb_subtree_gap >= length) {
				vma = right;
				continue;
			}
		}

		/* Go back up the rbtree to find next candidate node */
		while (true) {
			struct rb_node *prev = &vma->vm_rb;
			if (!rb_parent(prev))
				goto check_highest;
			vma = rb_entry(rb_parent(prev), struct vm_area_struct,
				       vm_rb);
			if (prev == vma->vm_rb.rb_left) {
				gap_start = vma->vm_prev->vm_end;
				gap_end = vma->vm_start;
				goto check_current;
			}
		}
	}

check_highest:
	gap_start = vi->highest_vm_end;
	gap_end = ULONG_MAX;
	if (gap_start > high_limit)
		return -ENOMEM;

found:
	/* We found a suitable gap. Clip it with the original low_limit. */
	if (gap_start < info->low_limit)
		gap_start = info->low_limit;
	return gap_start;
}

/*
 * Returns gap_end.
 */
static unsigned long __find_va_range_topdown(struct proc_info *proc,
					     struct vregion_info *vi,
					     struct vm_unmapped_area_info *info)
{
	struct vm_area_struct *vma;
	unsigned long length, low_limit, high_limit, gap_start, gap_end;

	length = info->length;
	gap_end = info->high_limit;
	if (gap_end < length) {
		dprintf_DEBUG("%#lx %#lx\n", gap_end, length);
		return -ENOMEM;
	}
	high_limit = gap_end - length;

	if (info->low_limit > high_limit) {
		dprintf_DEBUG("%#lx %#lx\n", info->low_limit, high_limit);
		return -ENOMEM;
	}
	low_limit = info->low_limit + length;

	/* Check highest gap, which does not precede any rbtree node */
	gap_start = vi->highest_vm_end;
	if (gap_start <= high_limit)
		goto found_highest;

	/* Check if rbtree root looks promising */
	if (RB_EMPTY_ROOT(&vi->mm_rb)) {
		dprintf_DEBUG("empty mm_rb root %d\n", 0);
		return -ENOMEM;
	}
	vma = rb_entry(vi->mm_rb.rb_node, struct vm_area_struct, vm_rb);
	if (vma->rb_subtree_gap < length) {
		dprintf_DEBUG("%#lx %#lx\n", vma->rb_subtree_gap, length);
		return -ENOMEM;
	}

	while (true) {
		/* Visit right subtree if it looks promising */
		gap_start = vma->vm_prev ? vma->vm_prev->vm_end : 0;
		if (gap_start <= high_limit && vma->vm_rb.rb_right) {
			struct vm_area_struct *right =
				rb_entry(vma->vm_rb.rb_right,
					 struct vm_area_struct, vm_rb);
			if (right->rb_subtree_gap >= length) {
				vma = right;
				continue;
			}
		}

	check_current:
		/* Check if current node has a suitable gap */
		gap_end = vma->vm_start;
		if (gap_end < low_limit)
			return -ENOMEM;
		if (gap_start <= high_limit && gap_end - gap_start >= length)
			goto found;

		/* Visit left subtree if it looks promising */
		if (vma->vm_rb.rb_left) {
			struct vm_area_struct *left =
				rb_entry(vma->vm_rb.rb_left,
					 struct vm_area_struct, vm_rb);
			if (left->rb_subtree_gap >= length) {
				vma = left;
				continue;
			}
		}

		/* Go back up the rbtree to find next candidate node */
		while (true) {
			struct rb_node *prev = &vma->vm_rb;
			if (!rb_parent(prev))
				return -ENOMEM;
			vma = rb_entry(rb_parent(prev), struct vm_area_struct,
				       vm_rb);
			if (prev == vma->vm_rb.rb_right) {
				gap_start =
					vma->vm_prev ? vma->vm_prev->vm_end : 0;
				goto check_current;
			}
		}
	}

found:
	/* We found a suitable gap. Clip it with the original high_limit. */
	if (gap_end > info->high_limit)
		gap_end = info->high_limit;

found_highest:
	/* Compute highest gap address at the desired alignment */
	gap_end -= info->length;
	BUG_ON(gap_end < info->low_limit);
	BUG_ON(gap_end < gap_start);
	return gap_end;
}

/*
 * This function will try to find a valid VA range within the given vRegion.
 * If found, return the starting VA.
 * 
 * Note that VMA tree is NOT changed within this function.
 */
unsigned long __find_va_range(struct proc_info *proc, struct vregion_info *vi,
			      unsigned long len)
{
	struct vm_unmapped_area_info info;
	unsigned long addr;
	unsigned long low_limit, high_limit;

	BUG_ON(!proc || !vi);

	if (len > VREGION_SIZE)
		return -ENOMEM;

	low_limit = vregion_to_start_va(proc, vi);
	high_limit = vregion_to_end_va(proc, vi);

	info.length = len;
	info.low_limit = low_limit;
	info.high_limit = high_limit;

	/*
	 * Default is topdown, set by init_vregion().
	 */
	if (vi->flags & VREGION_INFO_FLAG_UNMAPPED_AREA_TOPDOWN)
		addr = __find_va_range_topdown(proc, vi, &info);
	else
		addr = __find_va_range_bottomup(proc, vi, &info);

	if (addr > high_limit - len)
		return -ENOMEM;
	return addr;
}

/*
 * Allocate a virtual address range from a given vRegion.
 * The VMA tree will be updated.
 * This funtion will NOT touch other vRegions.
 *
 * vRegion lock is held upon entry.
 *
 * Return 0 Failure. Not -ENOMEM!!
 */
static unsigned long __alloc_va(struct proc_info *proc, struct vregion_info *vi,
				unsigned long len, unsigned long vm_flags)
{
	unsigned long addr;
	bool has_conflict;
	int nr_retry = 0;

	/*
	 * All length must be page aligned
	 * to cover a whole PTE range.
	 */
	len = PAGE_ALIGN(len);
	BUG_ON(len > VREGION_SIZE);

retry:
	/*
	 * Step 1:
	 * Go through the current VMA tree, find a range.
	 * Nothing will be updated during this one.
	 */
	addr = __find_va_range(proc, vi, len);
	if (IS_ERR_VALUE(addr)) {
		dprintf_ERROR("nr_retry=%d Fail to alloc VA range. PID %u vregion_idx %lu\n",
			nr_retry, proc->pid, vregion_to_index(proc, vi));
		return 0;
	}

	if (unlikely(!IS_ALIGNED(addr, PAGE_SIZE))) {
		dprintf_ERROR("addr %#lx not page aligned. why??\n", addr);
		return 0;
	}

	/*
	 * Step 2:
	 * Check if above [addr, addr+len] mapped PTEs
	 * can be allocated from the shadow pgtable.
	 */
	has_conflict = check_and_insert_shadow_conflicts(proc, vi, addr,
							 PAGE_SIZE, PAGE_SHIFT, /* XXX User defined? */
							 len);
	if (has_conflict) {
		nr_retry++;
		/*
		 * XXX
		 * which range?
		 * only a page, or the whole?? Maybe a page is enough?
		 */
		vma_tree_new(proc, vi, addr, len, LEGOMEM_VM_FLAGS_CONFLICT);

		dprintf_DEBUG("VA alloc Conflict [%#lx-%#lx] nr_retry: %d\n",
			addr, addr + len, nr_retry);
		goto retry;
	}

	/*
	 * Try to update the VMA tree with [addr, addr+len]
	 * No need to revert anything if it fails.
	 */
	addr = vma_tree_new(proc, vi, addr, len, vm_flags);
	if (IS_ERR_VALUE(addr)) {
		dprintf_ERROR("Fail to update VMA tree. PID %u vregion_idx %lu\n",
			proc->pid, vregion_to_index(proc, vi));
		return 0;
	}

	alloc_fpga_pte_range(proc, addr, addr + len, vm_flags, PAGE_SIZE);
	return addr;
}

unsigned long alloc_va_vregion(struct proc_info *proc, struct vregion_info *vi,
			       unsigned long len, unsigned long vm_flags)
{
	unsigned long addr;

	pthread_spin_lock(&vi->lock);
	addr = __alloc_va(proc, vi, len, vm_flags);
	pthread_spin_unlock(&vi->lock);

	return addr;
}

int free_va_vregion(struct proc_info *proc, struct vregion_info *vi,
	    unsigned long start, unsigned long len)
{
	int ret;

	pthread_spin_lock(&vi->lock);
	ret = __free_va(proc, vi, start, len);
	pthread_spin_unlock(&vi->lock);

	return ret;
}

unsigned long alloc_va(struct proc_info *proc, unsigned long len,
		       unsigned long vm_flags)
{
	struct vregion_info *vi;
	unsigned long addr;
	int nr_vregions;
	int i;

	if (!len || !proc)
		return -EINVAL;

	/*
	 * If the len only spans one vregion, we can select any regions.
	 * If it spans more than one vregions, we need to make sure
	 * they are contiguous. More specific, we need to control the
	 * topdown and bottomup behavior.
	 */
	nr_vregions = DIV_ROUND_UP(len, VREGION_SIZE);
	if (nr_vregions == 1) {
		/*
		 * TODO:
		 * Instead of scanning the array, we should keep
		 * a list of available vregions, i.e., assigned by monitor.
		 */
		for (i = 0; i < NR_VREGIONS; i++) {
			vi = proc->vregion + i;
			addr = alloc_va_vregion(proc, vi, len, vm_flags);
			if (!IS_ERR_VALUE(addr))
				return addr;
		}
	} else {
		printf("TODO\n");
		BUG();
	}

	return 0;
}

/*
 * Free range: [start, start+len).
 */
int free_va(struct proc_info *proc, unsigned long start, unsigned long len)
{
	int ret;
	struct vregion_info *vi;
	unsigned long vlen, end, vregion_end;

	if (!proc)
		return -EINVAL;

	if (!len || !start)
		return -EINVAL;

	end = start + len;
	if (end < start)
		return -EINVAL;

	/* Walk through a possible list of vRegions */
	while (start != end) {
		vi = va_to_vregion(proc, start);

		vregion_end = vregion_to_end_va(proc, vi);
		vlen = vregion_end - start;
		if (vlen > len)
			vlen = len;

		ret = free_va_vregion(proc, vi, start, vlen);
		if (ret)
			break;

		len -= vlen;
		start += vlen;
	}
	return ret;
}
