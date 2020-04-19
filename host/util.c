/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#define _GNU_SOURCE
#include <sched.h>
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
#include <dirent.h>
#include <net/if.h>

#include "core.h"
#include "net/net.h"

/*
 * The network device we are using.
 * This is passed by command line during startup
 */
char global_net_dev[32];

/*
 * Given the @ibvdev name, we will try to fill the @ndev.
 * This is Linux-specific and relys on /sys files.
 */
int ibdev2netdev(const char *ibdev, char *ndev, size_t ndev_buf_size)
{
	char dev_dir[128];
	DIR *dp;
	struct dirent *dirp;
	int ret;

	sprintf(dev_dir, "/sys/class/infiniband/%s/device/net", ibdev);

	dp = opendir(dev_dir);
	if (!dp)
		return errno;

	ret = -1;
	while (1) {
		dirp = readdir(dp);
		if (!dirp)
			break;

		if (!strcmp(dirp->d_name, "."))
			continue;
		if (!strcmp(dirp->d_name, ".."))
			continue;

		strncpy(ndev, dirp->d_name, ndev_buf_size);
		ret = 0;
		break;
	}
	closedir(dp);

	return ret;
}

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
 * @ip and @ip_str from caller in host order.
 * @dev is the network interface we are using, can be NULL.
 *
 * In return, we will fill @mac. Return value 0 means we have filled @mac,
 * otherwire it's a failure.
 */
int get_mac_of_remote_ip(int ip, char *ip_str, char *dev,
		unsigned char *mac)
{
	FILE *fp;
	char ping_cmd[128];
	char ip_neigh_cmd[128];
	char line[1024];
	int ret;
	unsigned char dev_mac[6];
	char dev_ip_str[INET_ADDRSTRLEN];
	int dev_ip;

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

	/* We found one from ip neigh */
	if (ret == 0)
		return 0;

	if (!dev)
		return -ENODEV;

	/* Last try: check if the IP matches device IP */
	ret = get_interface_mac_and_ip(dev, dev_mac, dev_ip_str, &dev_ip);
	if (ret)
		return ret;

	if (dev_ip == ip) {
		memcpy(mac, dev_mac, 6);
		return 0;
	}
	return -ENODEV;
}

/*
 * Return 0 on failure, otherwise a positive MTU value.
 */
unsigned int get_device_mtu(const char *dev)
{
	int fd, ret;
	struct ifreq ifr;

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return 0;

	ret = ioctl(fd, SIOCGIFMTU, &ifr);
	if (ret) {
		perror("ioctl mtu");
		ret = 0;
		goto out;
	}

	ret = ifr.ifr_mtu;

out:
	close(fd);
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
		char *ip_str, int *ip)
{
	int fd, ret;
	struct ifreq ifr;
	char str[INET_ADDRSTRLEN];
	struct in_addr in_addr;

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
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
	int ip;
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

	printf("\033\[34m[%s:%d]: Local Endpoint dev: %s IP: %s %x Port: %d MAC: ",
			__func__, __LINE__, dev, ip_str, ip, port);
	for (i = 0; i < 6; i++)
		printf("%x ", mac[i]);
	printf("\033[0m\n");

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

struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

/*
 * Open the local mgmt session
 * and its associated handler thread
 */
int init_local_management_session(void)
{
	struct endpoint_info dummy_ei = { 0 };

	/*
	 * This is LOCAL dummy board.
	 * We will never use the endpoint_info from this special bi,
	 * thus using a dummy_ei is fine.
	 */
	mgmt_dummy_board = add_board("special_local_mgmt", 0,
			&dummy_ei, &dummy_ei, true);
	if (!mgmt_dummy_board)
		return -ENOMEM;
	mgmt_dummy_board->flags |= BOARD_INFO_FLAGS_DUMMY;

	/* This is our LOCAL mgmt session */
	mgmt_session = get_board_mgmt_session(mgmt_dummy_board);

	/*
	 * If this is happening.. adjust your code, put
	 * this function before all other functions that might create sessions.
	 */
	if (get_local_session_id(mgmt_session) !=  LEGOMEM_MGMT_SESSION_ID) {
		dprintf_ERROR("mgmt_session id is %d, not 0.\n",
				get_local_session_id(mgmt_session));
		return -EINVAL;
	}
	return 0;
}

int pin_cpu(int cpu_id)
{
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id, &cpu_set);
	return sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
}
