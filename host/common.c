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

/*
 * Server side session handler..
 * just for debugging and measurement, i guess
 */
void *user_session_handler(void *_ses)
{
	struct thpool_buffer tb = { 0 };
	struct lego_header *lego_header;
	struct session_net *ses;
	int ret, opcode;
	int cpu, node;

	ses = (struct session_net *)_ses;

	tb.tx = malloc(THPOOL_BUFFER_SIZE);
	if (!tb.tx) {
		dprintf_ERROR("OOM %d\n", 0);
		return NULL;
	}

	ret = net_reg_send_buf(ses, tb.tx, THPOOL_BUFFER_SIZE);
	if (ret) {
		dprintf_ERROR("Fail to register TX buffer %d\n", ret);
		return NULL;
	}

	getcpu(&cpu, &node);
	dprintf_INFO("CPU=%d Node=%d, for session local_id = %u remote_id = %u\n",
		cpu, node, get_local_session_id(ses), get_remote_session_id(ses));

	while (1) {
		ret = net_receive_zerocopy(ses, &tb.rx, &tb.rx_size);
		if (ret <= 0)
			continue;

		lego_header = to_lego_header(&tb.rx);
		opcode = lego_header->opcode;
		switch (opcode) {
		case OP_REQ_PINGPONG:
			handle_pingpong(&tb);
			break;
		default:
			dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
				      opcode, legomem_opcode_str(opcode));
			break;
		}

		if (likely(!ThpoolBufferNoreply(&tb)))
			net_send(ses, tb.tx, tb.tx_size);
		tb.flags = 0;
	}
	return NULL;
}
