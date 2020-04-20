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
 * Server side session handler.
 * just for debugging and measurement, i guess
 *
 * The reason to have this handler: when a sender wants to open a new session
 * with a receiver, the receiver side user code does not know when this event
 * would happen and what's the session id without another layer of msg exchange.
 *
 * Thus, we took a different approach at receiver side: whenever a recever
 * gets a open_session request, it will proactively launch a new thread
 * running this handler (in generic_handle_close_session()).
 */
static int tmp_cpu = 1;
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

	pin_cpu(tmp_cpu++);
	getcpu(&cpu, &node);
	dprintf_INFO("CPU=%d Node=%d, for session local_id = %u remote_id = %u tb.tx=%#lx\n",
		cpu, node, get_local_session_id(ses), get_remote_session_id(ses), (unsigned long)tb.tx);

	while (1) {
		if (unlikely(ses_thread_should_stop(ses)))
			break;

		ret = net_receive_zerocopy(ses, &tb.rx, &tb.rx_size);
		if (ret <= 0)
			continue;

		lego_header = to_lego_header(tb.rx);
		opcode = lego_header->opcode;
		switch (opcode) {
		case OP_REQ_PINGPONG:
			handle_pingpong(&tb);
			break;
		default:
			dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
				      opcode, legomem_opcode_str(opcode));
			set_tb_tx_size(&tb, sizeof(struct legomem_common_headers));
			break;
		}

		if (likely(!ThpoolBufferNoreply(&tb)))
			net_send(ses, tb.tx, tb.tx_size);
		tb.flags = 0;
	}

	free(tb.tx);
	return NULL;
}
