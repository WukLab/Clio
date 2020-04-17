/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * This file includes some common shared handlers among host/monitor.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/thpool.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "core.h"
#include "net/net.h"

/*
 * Handle a pingpong request.
 * The reply size is specificed by sender, but we enforce a cap.
 */
void handle_pingpong(struct thpool_buffer *tb)
{
	struct legomem_pingpong_req *req;
	int reply_size;
#define HANDLE_PINGPONG_MAX_REPLY_SIZE (4096)

	req = (struct legomem_pingpong_req *)tb->rx;

	reply_size = req->reply_size + sizeof(struct legomem_pingpong_resp);
	if (reply_size > HANDLE_PINGPONG_MAX_REPLY_SIZE)
		reply_size = HANDLE_PINGPONG_MAX_REPLY_SIZE;
	set_tb_tx_size(tb, reply_size);
}
