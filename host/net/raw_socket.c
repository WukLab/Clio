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
static char eth_device[] = "ens4";

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

	prepare_headers(&ses_net->route, buf, buf_size);

	ret = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *)saddr, sizeof(*saddr));
	return ret;
}

/*
 * Return number of bytes received.
 * Return -1 if error.
 */
static int raw_socket_receive(struct session_net *ses_net,
			      void *buf, size_t buf_size)
{
	int ret;
	int sockfd;
	struct session_raw_socket *ses_socket;

	if (unlikely(!ses_net || !buf || !buf_size))
		return -EINVAL;

	ses_socket = (struct session_raw_socket *)ses_net->raw_net_private;
	sockfd = ses_socket->sockfd;

	ret = recvfrom(sockfd, buf, buf_size, 0, NULL, NULL);
	return ret;
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

static struct session_net *
init_raw_socket(struct endpoint_info *local_ei, struct endpoint_info *remote_ei)
{
	struct session_net *ses_net;
	struct session_raw_socket *ses_socket;
	int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct sockaddr_ll saddr;

	ses_net = malloc(sizeof(struct session_net));
	if (!ses_net)
		return NULL;

	ses_socket = malloc(sizeof(struct session_raw_socket));
	if (!ses_socket) {
		free(ses_net);
		return NULL;
	}
	ses_net->raw_net_private = ses_socket;

	sockfd = raw_socket_open(eth_device, &if_idx, &if_mac);
	if (sockfd < 0) {
		printf("Fail to open raw socket.\n");
		goto free;
	}

	/* Prepare session_socket */
	saddr.sll_ifindex = if_idx.ifr_ifindex;
	saddr.sll_halen = ETH_ALEN;
	memcpy(saddr.sll_addr, local_ei->mac, 6);

	ses_socket->sockfd = sockfd;
	ses_socket->if_idx = if_idx;
	ses_socket->if_mac = if_mac;
	ses_socket->saddr  = saddr;

	/* Prepare session_net */
	prepare_routing_info(&ses_net->route, local_ei, remote_ei);
	memcpy(&ses_net->local_ei, local_ei, sizeof(*local_ei));
	memcpy(&ses_net->remote_ei, remote_ei, sizeof(*remote_ei));

	return ses_net;
free:
	free(ses_net);
	free(ses_socket);
	return NULL;
}

/*
 * This function is used to demonstrate the
 * usages of raw socket and for testing purpose.
 */
void test_raw_socket(struct session_net *ses)
{
	void *send_buf;
	void *recv_buf;
	int buf_size;
	int i, nr_msg;

	buf_size = 128;
	send_buf = malloc(buf_size);
	recv_buf = malloc(buf_size);
	if (!send_buf || !recv_buf)
		return;
	memset(send_buf, 0, buf_size);
	memset(recv_buf, 0, buf_size);

	struct dummy_payload {
		unsigned long mark;
	};

	/*
	 * Send certain amount of msgs
	 * and wait for the reply, dump it.
	 */
	nr_msg = 10;
	for (i = 0; i < nr_msg; i++) {
		int ret;
		struct dummy_payload *dp;

		dp = send_buf + 44;
		dp->mark = i;
		raw_socket_send(ses, send_buf, buf_size);

		memset(recv_buf, 0, buf_size);
		ret = raw_socket_receive(ses, recv_buf, buf_size);

		dp = recv_buf + 44;
		printf("i %d ret: %d dp_mark=%lu\n", i, ret, dp->mark);
		dump_packet_headers(recv_buf);
	}
}

struct raw_net_ops raw_socket_ops = {
	.name			= "raw_socket",
	.send_one		= raw_socket_send,
	.receive_one		= raw_socket_receive,
	.receive_one_nb		= NULL,
	.init			= init_raw_socket,
};
