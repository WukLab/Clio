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

/*
 * This is the local endpoint info
 * Constructed during startup based on network device and UDP port used.
 */
struct endpoint_info default_local_ei;

struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

int init_local_management_session(void)
{
	struct endpoint_info dummy_ei;

	mgmt_dummy_board = add_board("special_local_mgmt", 0, &dummy_ei, &dummy_ei);
	if (!mgmt_dummy_board)
		return -ENOMEM;
	mgmt_session = get_board_mgmt_session(monitor_bi);
	return 0;
}

/*
 * Use input @monitor_addr to create a local network session with the monitor.
 * Note we just create local data structures for monitor's management session.
 * We do not need to contact monitor for this particular creation.
 */
int init_monitor_session(char *monitor_addr, struct endpoint_info *local_ei)
{
	unsigned int ip, port;
	unsigned char mac[6];
	struct endpoint_info monitor_ei;

	/*
	 * TODO
	 * 1) parse ip:port string
	 * 2) ask ARP to find out the MAC addr
	 */
	memcpy(monitor_ei.mac, mac, 6);
	monitor_ei.ip = ip;
	monitor_ei.udp_port = port;

	monitor_bi = add_board("special_monitor_mgmt", 0, &monitor_ei, local_ei);
	if (!monitor_bi)
		return -ENOMEM;

	monitor_session = get_board_mgmt_session(monitor_bi);
	return 0;
}

int init_default_local_ei(char *ndev, unsigned int port)
{
	/*
	 * TODO
	 * 1) check if ndev exist
	 * 2) get mac and IP addr
	 * 3) fill ei
	 */
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
	       "  ./host.o --monitor=\"127.0.0.1:8888\" --port 8887 --dev=\"lo\" \n"
	       "  ./host.o -m 127.0.0.1:8888 -p 8887 -d lo\n");
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

	/* Parse arguments */
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

	/* Check if required arguments are passed */
	if (!ndev_set) {
		printf("ERROR: Please specify the network device (Use ifconfig to check).\n\n");
		print_usage();
		return 0;
	}

	if (!monitor_addr_set) {
		printf("ERROR: Please specify the monitor address.\n\n");
		print_usage();
		return 0;
	}
	printf("monitor: %s ndev: %s port: %d\n", monitor_addr, ndev, port);

	/*
	 * Init the local endpoint info
	 * - mac, ip, port
	 * Use information based on ndev and port.
	 */
	ret = init_default_local_ei(ndev, port);
	if (ret) {
		printf("Fail to init local endpoint. ndev %s port %d\n",
			ndev, port);
		exit(-1);
	}

	ret = init_net(&default_local_ei);
	if (ret) {
		printf("Fail to init network layer.\n");
		exit(-1);
	}

	/*
	 * Mainly init spinlocks
	 * This must take place before any init operations
	 * invovling boards, session, etc.
	 */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	/* Create a local session for remote monitor's mgmt session */
	ret = init_monitor_session(monitor_addr, &default_local_ei);
	if (ret) {
		printf("Fail to init monitor session\n");
		exit(-1);
	}

	/* Open local mgmt session */
	ret = init_local_management_session();
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	dump_boards();

	struct endpoint_info *remote_ei = &ei_wuklab05;
	test_app(&default_local_ei, remote_ei);

	return 0;
}
