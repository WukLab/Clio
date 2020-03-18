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
 * Given a network-order @ip, find what's the MAC we should use to reach it.
 * - If it is directly connected, it would be remote NIC's mac
 * - If it is behind switches, it would be the directly attached switch
 * - If it is loopback, it would be 0.
 */
static int get_mac_of_remote_ip(unsigned int ip, char *ip_str,
				unsigned char *mac)
{
	FILE *fp;
	char ping_cmd[128];
	char ip_neigh_cmd[128];
	char line[1024];
	int ret;

	if (ip == 0x100007f) {
		/*
		 * Loopback test: 127.0.0.1
		 * MAC address would be 0
		 */
		memset(mac, 0, 6);
		return 0;
	}

	/*
	 * In case arp cache was empty,
	 * we run ping to get it discovred.
	 */
	sprintf(ping_cmd, "ping -c 1 %s", ip_str);
	fp = popen(ping_cmd, "r");
	if (!fp) {
		perror("popen ping");
		return -1;
	}
	pclose(fp);

	sprintf(ip_neigh_cmd, "ip neigh show %s", ip_str);
	fp = popen(ip_neigh_cmd, "r");
	if (!fp) {
		perror("popen ip neigh");
		return -1;
	}

	/*
	 * The output format of: ip neight show <ip>
	 * 192.168.1.5 dev p4p1 lladdr e4:1d:2d:e4:81:51 STALE
	 */
	ret = -1;
	while (fgets(line, sizeof(line), fp)) {
		char t_ip[32], t_d[8], t_name[32], t_a[8],
		     t_status[8];

		sscanf(line, "%s %s %s %s %x:%x:%x:%x:%x:%x %s\n",
			t_ip, t_d, t_name, t_a,
			(unsigned int *)&mac[0], (unsigned int *)&mac[1],
			(unsigned int *)&mac[2], (unsigned int *)&mac[3],
			(unsigned int *)&mac[4], (unsigned int *)&mac[5],
			t_status);

		ret = 0;
		break;
	}
	pclose(fp);
	return ret;
}

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
	ip = htonl(ip);

	in_addr.s_addr = ip;
	inet_ntop(AF_INET, &in_addr, ip_str, sizeof(ip_str));

	ret = get_mac_of_remote_ip(ip, ip_str, mac);
	if (ret)
		return ret;

	/*
	 * Now let's save the info
	 * into the global variables
	 */
	memcpy(monitor_ip_str, ip_str, INET_ADDRSTRLEN);
	monitor_ip_n = ip;

	memcpy(monitor_ei.mac, mac, 6);
	monitor_ei.ip = ip;
	monitor_ei.udp_port = port;

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

/* We will fill @mac, @ip_str, and @ip. */
static int get_interface_mac_and_ip(const char *dev, unsigned char *mac,
				    char *ip_str, unsigned int *ip)
{
	int fd, ret;
	struct ifreq ifr;
	char str[INET_ADDRSTRLEN];
	struct in_addr in_addr;

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
		return fd;

	/* Get MAC */
	ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
	if (ret) {
		perror("ioctl mac");
		goto out;
	}
	memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

	/* Get IP */
	ret = ioctl(fd, SIOCGIFADDR, &ifr);
	if (ret) {
		perror("ioctl ip");
		goto out;
	}
	in_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	inet_ntop(AF_INET, &in_addr, str, sizeof(str));

	memcpy(ip_str, str, INET_ADDRSTRLEN);
	*ip = in_addr.s_addr;

	ret = 0;
out:
	close(fd);
	return ret;
}

int init_default_local_ei(const char *dev, unsigned int port)
{
	unsigned char mac[6];
	char ip_str[INET_ADDRSTRLEN];
	unsigned int ip;
	int ret;
	int i;

	ret = get_interface_mac_and_ip(dev, mac, ip_str, &ip);
	if (ret)
		return ret;

	/* Fill in the default local ei */
	memcpy(default_local_ei.mac, mac, 6);
	memcpy(default_local_ei.ip_str, ip_str, INET_ADDRSTRLEN);
	default_local_ei.ip = ip;
	default_local_ei.udp_port = port;

	/* Debugging info */
	printf("%s(): Local Endpoint Info: DEV: %s IP: %s Port: %d %x MAC: ",
		__func__, dev, ip_str, port, ip);
	for (i = 0; i < 6; i++)
		printf("%x ", mac[i]);
	printf("\n");

	return 0;
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
