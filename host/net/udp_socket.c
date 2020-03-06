/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

/*
 * This file use UDP to send and receive packets.
 * It's treated as a raw network layer, only a single port is used.
 * It uses kernel network stack and is mainly for testing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <uapi/net_header.h>
#include "net.h"

/* The one and the only open UDP port :) */
static int udp_sockfd;

struct session_udp_socket {
	int sockfd;
	struct sockaddr_in remote_addr;
};

static int udp_socket_send(struct session_net *ses_net, void *buf,
			   size_t buf_size)
{
	int ret;
	int sockfd;
	struct sockaddr_in *remote_addr;
	struct session_udp_socket *ses_socket;

	if (unlikely(!ses_net || !buf || buf_size > sysctl_link_mtu))
		return -EINVAL;
	
	ses_socket = (struct session_udp_socket *)ses_net->raw_net_private;
	sockfd = ses_socket->sockfd;
	remote_addr = &ses_socket->remote_addr;

	/*
	 * We don't prepare headers and let kernel stack do it.
	 * In this way, the space reserved for headers in the buf is not used.
	 * Thus we shift the pointer to the payload location.
	 */
	buf += GBN_HEADER_OFFSET;
	buf_size -= GBN_HEADER_OFFSET;

	ret = sendto(sockfd, buf, buf_size, 0, (const struct sockaddr *)remote_addr,
		     sizeof(*remote_addr));
	return ret;
}

static int udp_socket_receive(void *buf, size_t buf_size)
{
	int ret;
	struct sockaddr_in remote_addr;
	socklen_t addr_len = sizeof(remote_addr);
	struct ipv4_hdr *ipv4_hdr;

	/*
	 * Packet received from kernel UDP does not have L2-L4 headers.
	 * Thus shift the buf to the start of GBN header.
	 */
	buf += GBN_HEADER_OFFSET;
	buf_size -= GBN_HEADER_OFFSET;

	/*
	 * We will recv packets from any IPs.
	 * The sender info is returned in remote_addr
	 */
	ret = recvfrom(udp_sockfd, buf, buf_size, 0,
		       (struct sockaddr *)&remote_addr, &addr_len);

	/*
	 * Caller will use the src IP address to distinguish
	 * the sender, thus we need to refill it.
	 */
	ipv4_hdr = to_ipv4_header(buf);
	ipv4_hdr->src_ip = htonl(remote_addr.sin_addr.s_addr);
	return ret;
}

static int udp_open_session(struct session_net *ses_net,
			    struct endpoint_info *local_ei,
			    struct endpoint_info *remote_ei)
{
	struct session_udp_socket *ses_udp;
	struct sockaddr_in remote_addr;

	if (udp_sockfd <= 0) {
		printf("%s(): UDP socket is not open.\n", __func__);
		return -EINVAL;
	}

	ses_udp = malloc(sizeof(*ses_udp));
	if (!ses_udp)
		return -ENOMEM;
	ses_net->raw_net_private = ses_udp;

	/* This info is required to send */
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(remote_ei->udp_port);
	remote_addr.sin_addr.s_addr = htonl(remote_ei->ip);
	ses_udp->remote_addr = remote_addr;
	ses_udp->sockfd = udp_sockfd;

	return 0;
}

static int udp_close_session(struct session_net *ses_net)
{
	struct session_udp_socket *ses_udp;

	ses_udp = (struct session_udp_socket *)ses_net->raw_net_private;
	free(ses_udp);
	return 0;
}

/*
 * We only open one UDP port at one host (decided by @local_ei)
 * We use this port to accept all traffics targeting this port (INADDR_ANY).
 */
static int udp_init_once(struct endpoint_info *local_ei)
{
	int fd, ret;
	struct sockaddr_in local_addr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("Fail to open UDP socket: ");
		return fd;
	}

	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(local_ei->udp_port);
	local_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(fd, (const struct sockaddr *)&local_addr, sizeof(local_addr));
	if (ret < 0) {
		perror("bind failed");
		close(fd);
		return ret;
	}

	udp_sockfd = fd;
	return 0;
}

static void udp_exit(void)
{
	if (udp_sockfd > 0)
		close(udp_sockfd);
}

struct raw_net_ops udp_socket_ops = {
	.name			= "udp_socket",

	.init_once		= udp_init_once,
	.exit			= udp_exit,

	.open_session		= udp_open_session,
	.close_session		= udp_close_session,

	.send_one		= udp_socket_send,
	.receive_one		= udp_socket_receive,
	.receive_one_nb		= NULL,
	.receive_one_zerocopy	= NULL,
}; 
