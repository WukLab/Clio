/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

/*
 * This file describes the page size and its friends.
 * Please only put macros and very generic stuff here.
 * This file maybe included by various parties.
 */

#ifndef _LEGOFPGA_UAPI_H_
#define _LEGOFPGA_UAPI_H_

#include <uapi/compiler.h>

/*
 * Parameter: virtual address space width in number of bits
 */
#define VIRTUAL_ADDR_SHIFT	(50)

/*
 * Current smallest page size is 4M
 */
#define PAGE_SHIFT		(22)
#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((unsigned long)(x) << PAGE_SHIFT)
#define PHYS_PFN(x)	((unsigned long)((x) >> PAGE_SHIFT))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	ALIGN(addr, PAGE_SIZE)

/* test whether an address is aligned to PAGE_SIZE */
#define PAGE_ALIGNED(addr)	IS_ALIGNED((unsigned long)(addr), PAGE_SIZE)

#define NR_VIRTUAL_PAGES	(unsigned long)((1UL) << (VIRTUAL_ADDR_SHIFT - PAGE_SHIFT))

#endif /* _LEGOFPGA_UAPI_H_ */
