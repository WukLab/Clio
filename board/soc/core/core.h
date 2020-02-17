#ifndef _LEGOPGA_BOARD_SOC_CORE_H_
#define _LEGOPGA_BOARD_SOC_CORE_H_

/* VM */
unsigned long alloc_va(struct proc_info *proc, struct vregion_info *vi,
		       unsigned long len, unsigned long permission,
		       unsigned long flags);

int free_va(struct proc_info *proc,
	struct vregion_info *vi, unsigned long start, unsigned long len);


#endif /* _LEGOPGA_BOARD_SOC_CORE_H_ */
