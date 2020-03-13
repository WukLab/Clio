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
#include "core.h"
#include "net/net.h"

/*
 * TODO
 *
 * We will remove thise manual coding later.
 * We will build a simple membership management during runtime.
 * A UDP port will receive and broadcast this info.
 */
struct endpoint_info ei_wuklab02 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
	.ip		= 0xc0a80102, /* 192.168.1.2 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab05 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xe4, 0x81, 0x51 },
	.ip		= 0xc0a80105, /* 192.168.1.5 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab06 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb3, 0x54, 0x11 },
	.ip		= 0xc0a80106, /* 192.168.1.6 */
	.udp_port	= 8888,
};
struct endpoint_info board_0 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
	.ip		= 0xc0a801c8, /* 192.168.1.200 */
	.udp_port	= 1234,
};
struct endpoint_info board_1 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0x00, 0x00 },
	.ip		= 0xc0a80180,
	.udp_port	= 1234,
};

void test_app(struct endpoint_info *, struct endpoint_info *);

struct legomem_context *mgmt_context;
struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

int init_management_session(void)
{
	struct endpoint_info dummy_ei;

	mgmt_dummy_board = add_board("local_mgmt", 0, &dummy_ei, &dummy_ei);
	if (!mgmt_dummy_board)
		return -ENOMEM;

	mgmt_context = legomem_open_context_mgmt();
	if (!mgmt_context)
		return -ENOMEM;

	mgmt_session = legomem_open_session_mgmt(mgmt_context, mgmt_dummy_board);
	if (!mgmt_session)
		return -ENOMEM;
	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	struct endpoint_info *local_ei = &ei_wuklab02;
	struct endpoint_info *remote_ei = &ei_wuklab05;

	ret = init_net(local_ei);
	if (ret) {
		printf("Fail to init network layer.\n");
		exit(-1);
	}

	/* Mainly init spinlocks */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	/* Open the mgmt session, aka session_0 */
	init_management_session();

	test_app(local_ei, remote_ei);

	return 0;
}
