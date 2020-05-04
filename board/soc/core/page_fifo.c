/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

/*
 * This file handles the free page FIFOs.
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
#include <stdatomic.h>
#include <sys/sysinfo.h>
#include "core.h"
#include "buddy.h"

/*
 * Polling the ACK FIFO, check its info,
 * and then refill the corresponding freepage FIFOs
 */
static void *freepage_fifo_poll_func(void *_unused)
{
	int cpu, node;

	legomem_getcpu(&cpu, &node);
	dprintf_INFO("FreePage Poll Thread running on CPU %d", cpu);

	while (1) {
		;
	}

	return NULL;
}

int init_freepage_fifo(void)
{
	pthread_t t;
	int ret;

	ret = pthread_create(&t, NULL, freepage_fifo_poll_func, NULL);
	if (ret) {
		dprintf_ERROR("Fail to launch the freepage list poll func %d\n", ret);
		exit(1);
	}
	return 0;
}
