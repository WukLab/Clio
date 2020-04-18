/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <getopt.h>
#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/hashtable.h>
#include <uapi/bitops.h>
#include <uapi/sched.h>
#include <uapi/thpool.h>
#include <uapi/opcode.h>
#include <uapi/net_header.h>
#include "core.h"

__used static void *watchdog_func(void *_unused)
{
	while (1) {
		sleep(10);
		dump_stats();
	}
	return NULL;
}

#define CONFIG_ENABLE_WATCHDOG
#ifdef CONFIG_ENABLE_WATCHDOG
int create_watchdog_thread(void)
{
	pthread_t t;
	int ret;

	ret = pthread_create(&t, NULL, watchdog_func, NULL);
	if (ret) {
		dprintf_ERROR("Fail to create watchdog thread%d\n", errno);
		exit(-1);
	}
	return 0;
}
#else
int create_watchdog_thread(void)
{
	return 0;
}
#endif
