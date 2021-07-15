/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _FPGA_PGTABLE_H_
#define _FPGA_PGTABLE_H_

#include <uapi/compiler.h>

#define FPGA_TAG_OFFSET			(20)
#define FPGA_BUCKET_OFFSET		(8)

/* 1TB */
#define PHYSICAL_MEM_SIZE_BITS		(40)
#define PHYSICAL_MEM_SIZE		(1UL<<PHYSICAL_MEM_SIZE_BITS)

#define FPGA_NUM_PTE_PER_BUCKET_BITS	(4)
#define FPGA_NUM_PTE_PER_BUCKET		(1UL<<FPGA_NUM_PTE_PER_BUCKET_BITS)

/*
 * 1<<1, a ratio of 2
 */
#define SCALE_FACTOR_SHIFT		(1)
#define FPGA_NUM_PGTABLE_BUCKETS_BITS	(PHYSICAL_MEM_SIZE_BITS - PAGE_SHIFT - FPGA_NUM_PTE_PER_BUCKET_BITS + SCALE_FACTOR_SHIFT)
#define FPGA_NUM_PGTABLE_BUCKETS	(1UL<<FPGA_NUM_PGTABLE_BUCKETS_BITS)

#define FPGA_BUCKET_NUMBER_LENGTH	(FPGA_NUM_PGTABLE_BUCKETS_BITS)
#define FPGA_NUM_TOTAL_PTES		(FPGA_NUM_PTE_PER_BUCKET*FPGA_NUM_PGTABLE_BUCKETS)

struct lego_mem_pte {
	uint64_t ppa : 60;
	uint8_t  page_type : 2;
	uint8_t  allocated : 1;
	uint8_t  valid : 1;
	uint64_t tag;
} __packed;

struct lego_mem_bucket {
	struct lego_mem_pte ptes[FPGA_NUM_PTE_PER_BUCKET];
} __packed;

#endif /* _FPGA_PGTABLE_H_ */
