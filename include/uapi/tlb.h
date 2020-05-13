/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#ifndef _LEGOFPGA_UAPI_H_
#define _LEGOFPGA_UAPI_H_

#include <uapi/page.h>

#define LEGOMEM_NR_TLB_ENTRIES	(128)

/*
 * Default is FIFO.
 */
enum tlb_eviction_policy {
	LEGOMEM_TLB_EVICTION_RANDOM,
	LEGOMEM_TLB_EVICTION_FIFO,
	LEGOMEM_TLB_EVICTION_LRU,
};

#endif /* _LEGOFPGA_UAPI_H_ */
