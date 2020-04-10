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
#define NR_SEND_BUF_SLOTS	(256)

#if 0
void *test_net(void *arg)
{
	void *recv_buf, *send_buf;
	int buf_size, i, ret;
	struct dummy_payload *payload;
	struct dummy_payload *send_buf_ring[NR_SEND_BUF_SLOTS];
	struct gbn_header *hdr;
	struct session_net *ses;

	ses = (struct session_net *)arg;
	buf_size = 256;

	recv_buf = malloc(buf_size);
	if (!recv_buf)
		return NULL;
	memset(recv_buf, 0, buf_size);

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++) {
		if (!(send_buf_ring[i] = malloc(buf_size)))
			return NULL;
		memset(send_buf_ring[i], 0, buf_size);
	}

	i = 0;

	/*
	 * Please tune this during testing.
	 * One side is server, another is client.
	 */
		/* Server, recv msg */
	while (1) {
		send_buf = send_buf_ring[i % NR_SEND_BUF_SLOTS];
		payload = send_buf + LEGO_HEADER_OFFSET;
		payload->mark = i;

		printf("send %d\n", i);
		ret = net_send(ses, send_buf, buf_size);
		if (ret <= 0) {
			printf("send error\n");
			return NULL;
		}

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
		if (i >= NR_MSG_PER_THREAD)
			break;
	}

	sleep(3);

	free(recv_buf);
	for (i = 0; i < NR_SEND_BUF_SLOTS; i++)
		free(send_buf_ring[i]);

	return NULL;
}
#endif

int test_legomem_board(char *board_ip_port_str)
{
	struct board_info *remote_board;
	struct session_net *remote_mgmt_session;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	int i;

	printf("%s(): test board %s\n", __func__, board_ip_port_str);

	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

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

	struct msg {
		struct legomem_read_write_req header;
		char data[60];
	} *req, *resp;

	req = malloc(sizeof(*req));
	resp = malloc(sizeof(*resp));

	struct lego_header *lego_header;
	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_PINGPONG;

	/* Register this buffer with NIC */
	net_reg_send_buf(remote_mgmt_session, req, sizeof(*req));

	struct timespec ts, te;
	int nr_tests;

	/* warmup */
	net_send_and_receive(remote_mgmt_session, req, sizeof(*req),
			     resp, sizeof(*resp));
#if 1
	nr_tests = 5;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < nr_tests; i++) {
		net_send_and_receive(remote_mgmt_session, req, sizeof(*req),
				     resp, sizeof(*resp));
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("RTT avg %f nano seconds\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) - 
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / nr_tests);
#endif

#if 0
	nr_tests = 1;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	for (i = 0; i < nr_tests; i++) {
		net_send(remote_mgmt_session, req, sizeof(*req));
	}
	clock_gettime(CLOCK_MONOTONIC, &te);

	printf("Send avg %f nano seconds\n",
		(((double)te.tv_sec*1.0e9 + te.tv_nsec) - 
		((double)ts.tv_sec*1.0e9 + ts.tv_nsec)) / nr_tests);
#endif

	return 0;
}
