
/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 *
 * This file describes the SoC side thpool workers and buffers:
 * they bridge SoC and FPGA together.
 */

#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/sched.h>
#include <uapi/net_header.h>
#include <uapi/thpool.h>
#include <uapi/lego_mem.h>
#include <fpga/lego_mem_ctrl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>

#include "dma.h"
#include "core.h"

void *devmem_stat_base;

void init_stat_mapping(void)
{
	void *p;
	size_t size;

	size = SOC_REGMAP_STAT_END - SOC_REGMAP_STAT_START,

	p = mmap(0, size,
		 PROT_READ | PROT_WRITE,
		 MAP_SHARED, devmem_fd,
		 SOC_REGMAP_STAT_START);
	if (p == MAP_FAILED) {
		dprintf_ERROR("Fail to mmap stat regmap. %d", errno);
		exit(0);
	}

	devmem_stat_base = p;
	dprintf_INFO("regmap stat mapped @[%#lx-%#lx] -> [%#lx-%#lx]\n",
		(u64)devmem_stat_base, (u64)devmem_stat_base + size,
		SOC_REGMAP_STAT_START, SOC_REGMAP_STAT_END);
}
