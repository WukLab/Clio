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
#include "core.h"
#include "net/net.h"
#include "endpoint.h"

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

	mgmt_session = legomem_open_session_mgmt(mgmt_dummy_board);
	if (!mgmt_session)
		return -ENOMEM;
	return 0;
}

static void print_usage(void)
{
	printf("Usage ./host.o [Options]\n"
	       "Options:\n"
	       "  --monitor=<ip:port>         Specify monitor addr in IP:Port format\n"
	       "  --dev=<name>                Specify the local network device\n"
	       "  --port=<port>               Specify the local UDP port\n"
	       "Examples:\n"
	       "  ./host --monitor=\"127.0.0.1:8888\" --port 8887 --dev=\"lo\" \n"
	       "  ./host -m 127.0.0.1:8888 -p 8887 -d lo\n");
}

static struct option long_options[] = {
	{ "monitor",	required_argument,	NULL,	'm'},
	{ "port",	required_argument,	NULL,	'p'},
	{ "dev",	required_argument,	NULL,	'd'},
	{ 0,		0,			0,	0  }
};

int main(int argc, char **argv)
{
	int ret;
	int c, option_index = 0;
	char monitor_addr[32];
	bool monitor_addr_set = false;
	char ndev[32];
	bool ndev_set = false;
	int port = 0;

	struct endpoint_info *local_ei = &ei_wuklab02;
	struct endpoint_info *remote_ei = &ei_wuklab05;

	while (1) {
		c = getopt_long(argc, argv, "m:p:d:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			strncpy(monitor_addr, optarg, sizeof(monitor_addr));
			monitor_addr_set = true;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			strncpy(ndev, optarg, sizeof(ndev));
			ndev_set = true;
			break;
		default:
			print_usage();
			exit(-1);
		}
	}

	if (!monitor_addr_set) {
		printf("ERROR: Please specify the monitor address.\n\n");
		print_usage();
		return 0;
	}

	if (!ndev_set) {
		printf("ERROR: Please specify the network device (Use ifconfig to check).\n\n");
		print_usage();
		return 0;
	}

	printf("monitor: %s ndev: %s port: %d\n", monitor_addr, ndev, port);

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

	/*
	 * For hosts, they will use this mgmt session
	 * to contact with monitor.
	 */
	monitor_session = mgmt_session;

	test_app(local_ei, remote_ei);

	return 0;
}
