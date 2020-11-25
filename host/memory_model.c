/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/page.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "net/net.h"

void mc_wait_and_set_dependency(struct session_net *ses,
				unsigned long __remote addr, size_t total_size)
{
}

void mc_clear_dependency(struct session_net *ses, unsigned long __remote addr,
			 size_t total_size)
{
}
