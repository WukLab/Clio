/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_session.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "net/net.h"

struct dummy_payload {
	unsigned long mark;
};

void test_net(struct session_net *ses)
{
#define NR_SEND_BUF_SLOTS	(256)

	void *send_buf;
	void *recv_buf;
	struct dummy_payload *send_buf_ring[NR_SEND_BUF_SLOTS];
	int buf_size, i, ret;
	bool server;
	int nr_msg;

	struct dummy_payload *payload;

	buf_size = 256;
	
	for (i = 0; i < NR_SEND_BUF_SLOTS; i++) {
		if(!(send_buf_ring[i] = malloc(buf_size)))
			return;
		memset(send_buf_ring[i], 0, buf_size);
	}

	recv_buf = malloc(buf_size);
	if (!recv_buf)
		return;
	memset(recv_buf, 0, buf_size);

	i = 0;

	nr_msg = 10;

	/*
	 * Please tune this during testing.
	 * One side is server, another is client.
	 */
	server = false;
	if (server) {
		/* Server, recv msg */
		while (1) {
			ret = net_receive(ses, recv_buf, buf_size);
			if (ret <= 0) {
				printf("receive error\n");
				return;
			}

			dump_packet_headers(recv_buf);
			payload = recv_buf + LEGO_HEADER_OFFSET;
			printf("Msg %d Payload mark: %lu\n", i, payload->mark);
			i++;
		}
	} else {
		/* Client, send msg */
		while (1) {
			send_buf = send_buf_ring[i%NR_SEND_BUF_SLOTS];
			payload = send_buf + LEGO_HEADER_OFFSET;
			payload->mark = i++;

			printf("send %d\n", i-1);
			ret = net_send(ses, send_buf, buf_size);
			if (ret <= 0) {
				printf("send error\n");
				return;
			}

			if (i >= nr_msg)
				break;
		}
	}

	sleep(3);

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++)
		free(send_buf_ring[i]);
	free(recv_buf);
}

void test_app(struct endpoint_info *local_ei, struct endpoint_info *remote_ei)
{
	struct legomem_context *ctx;
	struct session_net *ses_net;
	struct board_info *remote_board;

	/*
	 * Prepare the board, which should be done
	 * by the system in a real setting.
	 */
	remote_board = add_board("remote_0", 1024, remote_ei, local_ei);
	if (!remote_board)
		return;
	dump_boards();

	/*
	 * Step 1:
	 * Application's first step is to open a new context
	 */
	ctx = legomem_open_context();
	if (!ctx)
		return;
	dump_legomem_context();

	/*
	 * Step 2:
	 * Open a network session with a remote board (or normal linux)
	 */
	ses_net = legomem_open_session(ctx, remote_board);
	if (!ses_net)
		return;
	dump_net_sessions();

	/*
	 * Step 3
	 * Send packets. Code in net/core.c
	 */
	test_net(ses_net);
}
