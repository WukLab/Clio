/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _FPGA_PGTABLE_H_
#define _FPGA_PGTABLE_H_

#include <uapi/compiler.h>

#define FPGA_TAG_OFFSET			(20)
#define FPGA_BUCKET_OFFSET		(8)
#define FPGA_BUCKET_NUMBER_LENGTH	(9)

#define FPGA_NUM_PTE_PER_BUCKET		(16)
#define FPGA_NUM_PGTABLE_BUCKETS	(1024)

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
