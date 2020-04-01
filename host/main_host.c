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

/*
 * Use input @monitor_addr to create a local network session with the monitor.
 * Note we just create local data structures for monitor's management session.
 * We do not need to contact monitor for this particular creation.
 */
static int init_monitor_session(char *ndev, char *monitor_addr,
				struct endpoint_info *local_ei)
{
	char ip_str[INET_ADDRSTRLEN];
	int ip, port;
	int ip1, ip2, ip3, ip4;
	struct in_addr in_addr;
	unsigned char mac[6];
	struct endpoint_info monitor_ei;
	int ret, i;

	sscanf(monitor_addr, "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	in_addr.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in_addr, ip_str, sizeof(ip_str));

	ret = get_mac_of_remote_ip(ip, ip_str, ndev, mac);
	if (ret) {
		dprintf_ERROR("cannot get mac of ip %s\n", ip_str);
		return ret;
	}

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

	monitor_bi = add_board("monitor", 0,
			       &monitor_ei, local_ei,
			       false);
	if (!monitor_bi)
		return -ENOMEM;

	monitor_bi->flags |= BOARD_INFO_FLAGS_MONITOR;

	monitor_session = get_board_mgmt_session(monitor_bi);

	/* Debugging info */
	printf("%s(): monitor IP: %s Port: %d MAC: ",
		__func__, monitor_ip_str, port);
	for (i = 0; i < 6; i++)
		printf("%x ", mac[i]);
	printf("\n");
	return 0;
}

void test(void)
{
	struct msg {
		struct legomem_common_headers comm_headers;
		int cnt;
	} req;
	struct lego_header *lego_header;
	int i;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_TEST;

	for (i = 0; i < 100; i++) {
		req.cnt = i;
		net_send(monitor_session, &req, sizeof(req));
		printf("%s(): finished sending cnt %5d\n", __func__, req.cnt);
		//net_receive(monitor_session, &resp, sizeof(resp));
		//printf("%s(): finished receiving cnt %5d\n", __func__, req.cnt);
	}
}

static int join_cluster(void)
{
	struct legomem_membership_join_cluster_req req;
	struct legomem_membership_join_cluster_resp resp;
	struct lego_header *lego_header;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_MEMBERSHIP_JOIN_CLUSTER;

	req.op.mem_size_bytes = 0;
	req.op.type = BOARD_INFO_FLAGS_HOST;
	req.op.ei = default_local_ei;

	ret = net_send_and_receive(monitor_session, &req, sizeof(req),
				   &resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_ERROR("net error %d\n", ret);
		return -1;
	}

	if (resp.ret == 0)
		dprintf_INFO("Succefully joined cluster! (monitor: %s)\n",
			monitor_bi->name);
	else
		dprintf_ERROR("Fail to join cluster, monitor error %d\n",
			resp.ret);
	return resp.ret;
}

static void print_usage(void)
{
	printf("Usage ./host.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --monitor=<ip:port>         Specify monitor addr in IP:Port format\n"
	       "  --skip_join                 Do not contact monitor for cluster join\n"
	       "                              This is useful if there is no real monitor running\n"
	       "  --dev=<name>                Specify the local network device\n"
	       "  --port=<port>               Specify the local UDP port\n"
	       "  --add_board=<ip:port>       Manually add a remote board\n"
	       "  --run_test                  Run built-in tests"
	       "\n"
	       "Examples:\n"
	       "  ./host.o --monitor=\"127.0.0.1:8888\" --port 8887 --dev=\"lo\" \n"
	       "  ./host.o -m 127.0.0.1:8888 -p 8887 -d lo\n");
}

static struct option long_options[] = {
	{ "monitor",	required_argument,	NULL,	'm'},
	{ "skip_join",  no_argument,		NULL,	's'},
	{ "port",	required_argument,	NULL,	'p'},
	{ "dev",	required_argument,	NULL,	'd'},
	{ "add_board",	required_argument,	NULL,	'b'},
	{ "run_test",	no_argument,	NULL,	't'},
	{ 0,		0,			0,	0  }
};

int main(int argc, char **argv)
{
	int ret;
	int c, option_index = 0;
	char monitor_addr[32];
	bool monitor_addr_set = false;
	bool skip_join_set = false;
	char ndev[32];
	bool ndev_set = false;
	int port = 0;
	bool run_test = false;

	char board_addr[32];
	char board_addr_set = false;

	/* Parse arguments */
	while (1) {
		c = getopt_long(argc, argv, "m:p:d:b:t",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			strncpy(monitor_addr, optarg, sizeof(monitor_addr));
			monitor_addr_set = true;
			break;
		case 's': {
			skip_join_set = true;
			break;
		}
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			strncpy(ndev, optarg, sizeof(ndev));
			strncpy(global_net_dev, optarg, sizeof(global_net_dev));
			ndev_set = true;
			break;
		case 'b':
			strncpy(board_addr, optarg, sizeof(board_addr));
			board_addr_set = true;
			break;
		case 't':
			run_test = true;
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
		printf("ERROR: Please specify the monitor address\n\n");
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

	/*
	 * Create a local session for remote monitor's mgmt session
	 * A special monitor board_info is added as well
	 */
	ret = init_monitor_session(ndev, monitor_addr, &default_local_ei);
	if (ret) {
		printf("Fail to init monitor session\n");
		exit(-1);
	}

	/* Add the special localhost board_info */
	add_localhost_bi(&default_local_ei);
 
	/* Open the local mgmt session */
	ret = init_local_management_session(true);
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	/*
	 * We only contact monitor
	 * if the --skip_join flag is NOT passed
	 */
	if (!skip_join_set) {
		ret = join_cluster();
		if (ret)
			exit(-1);
	} else {
		dprintf_INFO("Skipped join_cluster, monitor (%s) is not contacted.\n",
			monitor_bi->name);
	}

	/*
	 * Manually add some remote boards
	 * Do this after we've joined cluster
	 */
	if (board_addr_set) {
		manually_add_new_node_str(board_addr, BOARD_INFO_FLAGS_BOARD);
	}

	if (run_test) {
		if (board_addr_set)
			ret = test_legomem_board(board_addr);

		//ret = test_legomem_session();
		ret = test_legomem_migration();
	}

	pthread_join(mgmt_session->thread, NULL);
	return 0;
}
