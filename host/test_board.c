/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing: communication with board.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

struct dummy_payload {
	unsigned long mark;
};

#define NR_MSG_PER_THREAD	(10)
#define NR_SESSIONS		(3)

void send_msg(struct session_net *ses)
{
#define NR_SEND_BUF_SLOTS	(256)
	void *send_buf;
	struct dummy_payload *send_buf_ring[NR_SEND_BUF_SLOTS];
	int buf_size, i, ret;
	struct dummy_payload *payload;

	buf_size = 256;

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++) {
		if (!(send_buf_ring[i] = malloc(buf_size)))
			return;
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
			return;
		}

		if (i >= NR_MSG_PER_THREAD)
			break;
	}

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++)
		free(send_buf_ring[i]);
	
	return;
}

void *test_net(void *arg)
{
	void *recv_buf;
	int buf_size, i, ret;
	bool server;
	struct dummy_payload *payload;
	struct gbn_header *hdr;
	struct session_net *ses;

	ses = (struct session_net *)arg;
	buf_size = 256;

	recv_buf = malloc(buf_size);
	if (!recv_buf)
		return NULL;
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
				return NULL;
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
		send_msg(ses);
	}

	sleep(3);

	free(recv_buf);

	return NULL;
}

int test_legomem_board(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net **ses_net;
	struct session_net *remote_mgmt_session;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	pthread_t session_thread[NR_SESSIONS];
	int i;

	printf("%s(): test board %s\n", __func__, board_ip_port_str);

	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	/*
	 * Step II
	 * Find the testing board
	 */
	remote_board = find_board(ip, port);
	if (!remote_board) {
		dprintf_ERROR("Couldn't find the board_info for %s\n",
			board_ip_port_str);
		dump_boards();
		return -1;
	}
	printf("%s(): Using board %s\n", __func__, remote_board->name);

	/* Get our local endpoint for remote board's mgmt session */
	remote_mgmt_session = get_board_mgmt_session(remote_board);
	BUG_ON(!remote_mgmt_session);

	/*
	 * Step III.1
	 * Talk with remote board's mgmt session
	 */

#if 0
	 struct msg {
	 	struct legomem_common_headers comm_headers;
		int cnt;
	 } req, resp;

	 /*
	  * Send msg to remote mgmt session
	  */
	 net_send_and_receive(remote_mgmt_session, &req, sizeof(req),
			 &resp, sizeof(rsp));
#endif

	/*
	 * Step III.2
	 * Start test: open a lot sessions
	 */
	ses_net = malloc(NR_SESSIONS * sizeof(*ses_net));
	if (!ses_net)
		return -1;
	memset(ses_net, 0, NR_SESSIONS * sizeof(*ses_net));

	for (i = 0; i < NR_SESSIONS; i++) {
		ses_net[i] = legomem_open_session(NULL, remote_board);
		if (!ses_net[i]) {
			dprintf_ERROR("Fail to created the %dth session\n", i);
			return -1;
		}
	}

	/*
	 * Step III.3
	 * Send messages to board
	 */
	for (i = 0; i < NR_SESSIONS; i++)
		pthread_create(&session_thread[i], NULL, test_net, ses_net[i]);

	for (i = 0; i < NR_SESSIONS; i++)
		pthread_join(session_thread[i], NULL);

	/*
	 * Step III.4
	 * End test: close sessions
	 */
	// for (i = 0; i < NR_SESSIONS; i++)
	// 	if (ses_net[i])
	// 		legomem_close_session(NULL, ses_net[i]);

	return 0;
}
