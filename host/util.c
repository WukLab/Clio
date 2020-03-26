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
 * Given the host order @ip, fill in the @ip_str
 * @ip_str must be INET_ADDRSTRLEN bytes long.
 */
int get_ip_str(unsigned int ip, char *ip_str)
{
	struct in_addr in_addr;

	in_addr.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in_addr, ip_str, INET_ADDRSTRLEN);
	return 0;
}

/*
 * Given a network-order @ip, find what's the MAC we should use to reach it.
 * - If it is directly connected, it would be remote NIC's mac
 * - If it is behind switches, it would be the directly attached switch
 * - If it is loopback, it would be 0.
 *
 * @ip and @ip_str from caller in host order, we will fill @mac.
 */
int get_mac_of_remote_ip(unsigned int ip, char *ip_str, unsigned char *mac)
{
	FILE *fp;
	char ping_cmd[128];
	char ip_neigh_cmd[128];
	char line[1024];
	int ret;

	if (ip == 0x7f000001) {
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
 * Caller only provides @dev,
 * we will fill @mac, @ip_str, and @ip.
 *
 * @mac buffer must be 6 bytes long.
 * @ip_str buffer must be INET_ADDRSTRLEN bytes long.
 * @ip will be host order.
 */
int get_interface_mac_and_ip(const char *dev, unsigned char *mac,
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

	/* Get IP, in net order */
	ret = ioctl(fd, SIOCGIFADDR, &ifr);
	if (ret) {
		perror("ioctl ip");
		goto out;
	}

	in_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	inet_ntop(AF_INET, &in_addr, str, sizeof(str));

	memcpy(ip_str, str, INET_ADDRSTRLEN);

	/* Return the addr in host order */
	*ip = ntohl(in_addr.s_addr);

	ret = 0;
out:
	close(fd);
	return ret;
}

/*
 * This is the local endpoint info
 * Constructed during startup based on network device and UDP port used.
 */
struct endpoint_info default_local_ei;
struct board_info *default_local_bi;

/*
 * Give us a @dev and port@, we will init the ei for you.
 * Mac, IP, port, we will take care of them.
 */
int init_default_local_ei(const char *dev, unsigned int port,
			  struct endpoint_info *ei)
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
	memcpy(ei->mac, mac, 6);
	memcpy(ei->ip_str, ip_str, INET_ADDRSTRLEN);
	ei->ip = ip;
	ei->udp_port = port;

	/* Debugging info */
	printf("%s(): Local Endpoint dev: %s IP: %s %x Port: %d MAC: ",
		__func__, dev, ip_str, ip, port);
	for (i = 0; i < 6; i++)
		printf("%x ", mac[i]);
	printf("\n");

	return 0;
}

int add_localhost_bi(struct endpoint_info *ei)
{
	default_local_bi = add_board("special_localhost", 0, ei, ei, true);
	if (!default_local_bi)
		return -ENOMEM;

	default_local_bi->flags |= BOARD_INFO_FLAGS_LOCALHOST;
	return 0;
}
