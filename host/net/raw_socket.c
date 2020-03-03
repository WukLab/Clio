/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

/*
 * This file describes the raw socket transport layer.
 * It uses kernel socket interface and it's not kernel-bypassing.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <uapi/net_header.h>
#include "net.h"

/*
 * This is the ethernet device name we are using.
 * It usually is: p4p1, or ens4 in our machines.
 */
static char eth_device[] = "p4p1";

/* Those are cached info for each session */
static int raw_sockfd;
static struct ifreq raw_if_idx;
static struct ifreq raw_if_mac;
static struct sockaddr_ll raw_saddr;

struct session_raw_socket {
	int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct sockaddr_ll saddr;
};

/*
 * Return number of bytes sent out
 * Return -1 if error 
 */
static int raw_socket_send(struct session_net *ses_net,
			   void *buf, size_t buf_size)
{
	int ret;
	int sockfd;
	struct sockaddr_ll *saddr;
	struct session_raw_socket *ses_socket;

	if (unlikely(!ses_net || !buf || buf_size > sysctl_link_mtu))
		return -EINVAL;

	ses_socket = (struct session_raw_socket *)ses_net->raw_net_private;
	sockfd = ses_socket->sockfd;
	saddr = &ses_socket->saddr;

	/* Cook the L2-L4 layer headers */
	prepare_headers(&ses_net->route, buf, buf_size);

	ret = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *)saddr, sizeof(*saddr));
	return ret;
}

static int raw_socket_receive(void *buf, size_t buf_size)
{
	int ret;

	/*
	 * TODO
	 * Similar to UDP socket, can we get the remote addr here?
	 */
	ret = recvfrom(raw_sockfd, buf, buf_size, 0, NULL, NULL);
	return ret;
}

static int raw_socket_open_session(struct session_net *ses_net,
				   struct endpoint_info *local_ei,
				   struct endpoint_info *remote_ei)
{
	struct session_raw_socket *ses_socket;

	if (raw_sockfd <= 0) {
		printf("%s(): socket was not open.\n", __func__);
		return -EINVAL;
	}

	ses_socket = malloc(sizeof(struct session_raw_socket));
	if (!ses_socket)
		return -ENOMEM;
	ses_net->raw_net_private = ses_socket;

	ses_socket->sockfd = raw_sockfd;
	ses_socket->if_idx = raw_if_idx;
	ses_socket->if_mac = raw_if_mac;
	ses_socket->saddr  = raw_saddr;

	return 0;
}

static int raw_socket_close_session(struct session_net *ses_net)
{
	struct session_raw_socket *ses_socket;

	ses_socket = (struct session_raw_socket *)ses_net->raw_net_private;
	free(ses_socket);
	return 0;
}

/*
 * Open a raw socket file descriptor.
 * It will return the socket fd, and fill the parameters.
 *
 * Return positive sockfd on success, otherwise negtive error values.
 */
static int raw_socket_open(char *if_name, struct ifreq *if_idx,
			   struct ifreq *if_mac)
{
	int sockfd;
	int ret;

	if (unlikely(!if_name || !if_idx || !if_mac))
		return -EINVAL;

	/*
	 * XXX
	 * IPPROTO_RAW seems to be promiscuous mode.
	 * sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	 */
	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sockfd < 0) {
		perror("socket");
		return sockfd;
	}

	/* Get the index of the interface to send on */
	memset(if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx->ifr_name, if_name, IFNAMSIZ);
	ret = ioctl(sockfd, SIOCGIFINDEX, if_idx);
	if (ret < 0) {
		printf("Fail to open device %s. Use ifconfig to check.\n", if_name);
		return ret;
	}

	/* Get the MAC address of the interface to send on */
	memset(if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac->ifr_name, if_name, IFNAMSIZ);
	ret = ioctl(sockfd, SIOCGIFHWADDR, if_mac);
	if (ret < 0) {
		perror("SIOCGIFHWADDR");
		return ret;
	}
	return sockfd;
}

static int raw_socket_init_once(struct endpoint_info *local_ei)
{
	int fd;
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct sockaddr_ll saddr;

	fd = raw_socket_open(eth_device, &if_idx, &if_mac);
	if (fd < 0) {
		printf("Fail to open raw socket.\n");
		return fd;
	}

	saddr.sll_ifindex = if_idx.ifr_ifindex;
	saddr.sll_halen = ETH_ALEN;
	memcpy(saddr.sll_addr, local_ei->mac, 6);

	raw_if_idx = if_idx;
	raw_if_mac = if_mac;
	raw_saddr = saddr;
	raw_sockfd = fd;

	return 0;
}

static void raw_socket_exit(void)
{
	if (raw_sockfd > 0)
		close(raw_sockfd);
}

struct raw_net_ops raw_socket_ops = {
	.name			= "raw_socket",

	.init_once		= raw_socket_init_once,
	.exit			= raw_socket_exit,

	.open_session		= raw_socket_open_session,
	.close_session		= raw_socket_close_session,

	.send_one		= raw_socket_send,
	.receive_one		= raw_socket_receive,
	.receive_one_nb		= NULL,
	.receive_one_zerocopy	= NULL,
};
