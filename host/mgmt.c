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

/* This request needs no reply */
static void handle_new_node(void *rx)
{
	struct legomem_membership_new_node_req *req;
	struct endpoint_info *new_ei;
	struct board_info *bi;
	int ret, i;
	unsigned char mac[6];
	unsigned int ip;
	char *ip_str;

	req = (struct legomem_membership_new_node_req *)rx;
	new_ei = &req->op.ei;

	/* Sanity check */
	if (req->op.type != BOARD_INFO_FLAGS_HOST &&
	    req->op.type != BOARD_INFO_FLAGS_BOARD) {
		printf("%s(): invalid type: %lu\n", __func__, req->op.type);
		return;
	}

	/*
	 * We may use a different local MAC address to reach the new host
	 * run our local ARP protocol to get the latest and update if necessary.
	 */
	ip = new_ei->ip;
	ip_str = (char *)new_ei->ip_str;
	ret = get_mac_of_remote_ip(ip, ip_str, mac);
	if (ret) {
		printf("%s(): fail to get the mac of new node.\n",
			__func__);
		return;
	}

	if (memcmp(mac, new_ei->mac, 6)) {
		printf("%s(): INFO mac updated ", __func__);
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", new_ei->mac[i]);
			else
				printf("%x -> ", new_ei->mac[i]);
		}
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", mac[i]);
			else
				printf("%x\n", mac[i]);
		}
		
		memcpy(new_ei->mac, mac, 6);
	}

	/* Finally add the board to the system */
	bi = add_board(req->op.name, req->op.mem_size_bytes,
		       new_ei, &default_local_ei, false);
	if (!bi)
		return;
	bi->flags = req->op.type;

	dprintf_INFO("new node added name: %s, ip:port: %s:%d type: %s\n",
		req->op.name, new_ei->ip_str, new_ei->udp_port,
		board_info_type_str(bi));

	dump_boards();
	dump_net_sessions();
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
	struct endpoint_info dummy_ei = { 0 };
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
