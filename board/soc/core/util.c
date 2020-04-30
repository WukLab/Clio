/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 * Some misc helper functions used by everyone, anywhere they want.
 */

#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <net/if.h>

#include "core.h"

int pin_cpu(int cpu_id)
{
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id, &cpu_set);
	return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
}

void getcpu(int *cpu, int *node)
{
	syscall(SYS_getcpu, cpu, node, NULL);
}
