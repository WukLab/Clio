/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
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
#include <getopt.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "core.h"
#include "net/net.h"

struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

static void handle_new_node(void *rx)
{
	struct legomem_membership_new_node_req *req;
	struct endpoint_info *ei;

	req = (struct legomem_membership_new_node_req *)rx;
	ei = &req->op.ei;

	printf("%s(): new node name: %s, ip:port: %s:%d\n",
		__func__, req->op.name, ei->ip_str, ei->udp_port);
}

static void *mgmt_handler_func(void *_unused)
{
	int ret;
	uint16_t opcode;
	size_t max_buf_size;
	void *rx;
	struct lego_header *lego_hdr;

	max_buf_size = sysctl_link_mtu;
	rx = malloc(max_buf_size);
	if (!rx)
		return NULL;

	while (1) {
		ret = net_receive(mgmt_session, rx, max_buf_size);
		if (ret <= 0)
			continue;

		lego_hdr = to_lego_header(rx);
		opcode = lego_hdr->opcode;

		switch (opcode) {
		case OP_REQ_MEMBERSHIP_NEW_NODE:
			handle_new_node(rx);
			break;
		default:
			printf("%s(): unknown opcode %u\n",
				__func__, opcode);
			break;
		}
	}
}

/*
 * Open the local mgmt session
 */
int init_local_management_session(bool create_mgmt_thread)
{
	struct endpoint_info dummy_ei;
	pthread_t t;
	int ret;

	/* This is LOCAL dummy board */
	mgmt_dummy_board = add_board("special_local_mgmt", 0,
				     &dummy_ei, &dummy_ei,
				     true);
	if (!mgmt_dummy_board)
		return -ENOMEM;
	mgmt_dummy_board->flags |= BOARD_INFO_FLAGS_DUMMY;

	/* This is our LOCAL mgmt session */
	mgmt_session = get_board_mgmt_session(mgmt_dummy_board);

	if (!create_mgmt_thread)
		return 0;

	/*
	 * Create the mgmt thread.
	 */
	ret = pthread_create(&t, NULL, mgmt_handler_func, NULL);
	if (ret) {
		printf("%s(): fail to create the mgmt handler thread\n", __func__);
		return ret;
	}

	/* Save into session-local storage */
	mgmt_session->thread = t;
	return 0;
}
