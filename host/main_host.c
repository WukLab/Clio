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
#include "endpoint.h"

/*
 * This is the local endpoint info
 * Constructed during startup based on network device and UDP port used.
 */
struct endpoint_info default_local_ei;

/*
 * Use input @monitor_addr to create a local network session with the monitor.
 * Note we just create local data structures for monitor's management session.
 * We do not need to contact monitor for this particular creation.
 */
static int init_monitor_session(char *monitor_addr, struct endpoint_info *local_ei)
{
	char ip_str[INET_ADDRSTRLEN];
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	struct in_addr in_addr;
	unsigned char mac[6];
	struct endpoint_info monitor_ei;
	int ret, i;

	sscanf(monitor_addr, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	in_addr.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in_addr, ip_str, sizeof(ip_str));

	ret = get_mac_of_remote_ip(ip, ip_str, mac);
	if (ret)
		return ret;

	/*
	 * Now let's save the info
	 * into the global variables
	 */
	memcpy(monitor_ip_str, ip_str, INET_ADDRSTRLEN);
	monitor_ip_h = ip;

	/* EI needs host order ip */
	monitor_ei.ip = ip;
	monitor_ei.udp_port = port;
	memcpy(monitor_ei.mac, mac, 6);

	/* The local_ei was constructed before */
	monitor_bi = add_board("special_monitor_mgmt", 0, &monitor_ei, local_ei);
	if (!monitor_bi)
		return -ENOMEM;

	monitor_session = get_board_mgmt_session(monitor_bi);

	/* Debugging info */
	printf("%s(): monitor IP: %s Port: %d MAC: ",
		__func__, monitor_ip_str, port);
	for (i = 0; i < 6; i++)
		printf("%x ", mac[i]);
	printf("\n");
	return 0;
}

static void join_cluster(void)
{
	struct msg {
		struct legomem_common_headers comm_headers;
		int cnt;
	} msg;
	struct reply {
		struct legomem_common_headers comm_headers;
		int cnt;
	} reply;
	int ret;
	int i;

	for (i = 0; i < 20; i++) {
		ret = net_send(monitor_session, &msg, sizeof(msg));
		if (ret <= 0)
			printf("%d fail to send\n", i);
	}
}

static void print_usage(void)
{
	printf("Usage ./host.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --monitor=<ip:port>         Specify monitor addr in IP:Port format\n"
	       "  --dev=<name>                Specify the local network device\n"
	       "  --port=<port>               Specify the local UDP port\n"
	       "\n"
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

void test_app(struct endpoint_info *, struct endpoint_info *);

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
	ret = init_default_local_ei(ndev, port, &default_local_ei);
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
	dump_net_sessions();

	join_cluster();

	struct endpoint_info *remote_ei = &ei_wuklab05;
	test_app(&default_local_ei, remote_ei);

	return 0;
}
