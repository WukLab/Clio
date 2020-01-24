/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#ifndef _INCLUDE_UAPI_VREGION_
#define _INCLUDE_UAPI_VREGION_

#define VREGION_SIZE_SHIFT	(30)
#define VREGION_SIZE		(1 << VREGION_SIZE_SHIFT)

struct vregion {
	unsigned int flags;
	unsigned int board_id;
};

#endif /* _INCLUDE_UAPI_VREGION_ */
