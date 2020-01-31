/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _INCLUDE_UAPI_VREGION_
#define _INCLUDE_UAPI_VREGION_

/*
 * Parameter: virtual address space width in number of bits
 */
#define VIRTUAL_ADDR_SHIFT	(40)

/*
 * Parameter: vRegion size in number of bits
 */
#define VREGION_SIZE_SHIFT	(30)
#define VREGION_SIZE		(1 << VREGION_SIZE_SHIFT)

#define NR_VREGIONS_SHIFT 	(VIRTUAL_ADDR_SHIFT-VREGION_SIZE_SHIFT)
#define NR_VREGIONS		(1 << NR_VREGIONS_SHIFT)

struct vregion_info {
	unsigned int flags;
	unsigned long board_id;
};

#endif /* _INCLUDE_UAPI_VREGION_ */
