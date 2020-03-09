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

#define NR_TEST_SEND_THREAD	(5)
#define NR_MSG_PER_THREAD	(100)

void *send_msg(void *arg)
{
#define NR_SEND_BUF_SLOTS	(256)
	void *send_buf;
	struct dummy_payload *send_buf_ring[NR_SEND_BUF_SLOTS];
	int buf_size, i, ret;
	struct dummy_payload *payload;
	struct session_net *ses;

	buf_size = 256;
	ses = (struct session_net *)arg;

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++) {
		if (!(send_buf_ring[i] = malloc(buf_size)))
			return NULL;
		memset(send_buf_ring[i], 0, buf_size);
	}

	i = 0;

	while (1) {
		send_buf = send_buf_ring[i % NR_SEND_BUF_SLOTS];
		payload = send_buf + LEGO_HEADER_OFFSET;
		payload->mark = i++;

		printf("send %d\n", i - 1);
		ret = net_send(ses, send_buf, buf_size);
		if (ret <= 0) {
			printf("send error\n");
			return NULL;
		}

		if (i >= NR_MSG_PER_THREAD)
			break;
	}

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++)
		free(send_buf_ring[i]);
	
	return NULL;
}

void test_net(struct session_net *ses)
{
	void *recv_buf;
	int buf_size, i, ret;
	bool server;
	struct dummy_payload *payload;
	struct gbn_header *hdr;
	pthread_t send_thread[NR_TEST_SEND_THREAD];

	buf_size = 256;

	recv_buf = malloc(buf_size);
	if (!recv_buf)
		return;
	memset(recv_buf, 0, buf_size);

	i = 0;

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

			hdr = recv_buf + GBN_HEADER_OFFSET;
			payload = recv_buf + LEGO_HEADER_OFFSET;
			printf("Msg %d Payload mark: %lu\n", i, payload->mark);
			/* seqnum starts from 1 */
			if (hdr->seqnum != i + 1) {
				printf("Receive out of order. Expected seq#: %d, Received seq#: %d\n",
				       i + 1, hdr->seqnum);
			}
			i++;
		}
	} else {
		/* Client, send msg */
		for (i = 0; i < NR_TEST_SEND_THREAD; i++)
			pthread_create(&send_thread[i], NULL, send_msg, ses);
	}

	for (i = 0; i < NR_TEST_SEND_THREAD; i++)
		pthread_join(send_thread[i], NULL);

	sleep(3);

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
