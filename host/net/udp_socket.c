/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

/*
 * This file describes the udp socket transport layer.
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
	 * We don't prepare headers and let kernel network stack prepare headers.
	 * In this way, the space reserved for headers in the buf is no use.
	 * thus we skip it and move the pointer to the actual payload
	 */
	buf += GBN_HEADER_OFFSET;
	buf_size -= GBN_HEADER_OFFSET;

	ret = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *)remote_addr,
		     sizeof(*remote_addr));
	printf("send %d bytes.\n", ret);
	return ret;
}

static int udp_socket_receive(struct session_net *ses_net, void *buf,
			      size_t buf_size)
{
	int ret;
	int sockfd;
	struct session_udp_socket *ses_socket;
	struct sockaddr_in *remote_addr;
	socklen_t addr_len;

	if (unlikely(!ses_net || !buf || !buf_size))
		return -EINVAL;
	
	ses_socket = (struct session_udp_socket *)ses_net->raw_net_private;
	sockfd = ses_socket->sockfd;
	remote_addr = &ses_socket->remote_addr;
	addr_len = sizeof(*remote_addr);

	/*
	 * Payload received from UDP doesn't have L2~L4 headers
	 * so put it after the go-back-N header offset
	 */
	buf += GBN_HEADER_OFFSET;
	buf_size -= GBN_HEADER_OFFSET;

	ret = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr *)remote_addr, &addr_len);
	printf("receive %d bytes.\n", ret);
	return ret;
}

static int udp_socket_open()
{
	int sockfd;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		perror("open udp socket");
	return sockfd;
}

static int udp_socket_bind(int sockfd, struct sockaddr_in *local_addr)
{
	int ret;

	if ((ret = bind(sockfd, (struct sockaddr *)local_addr, sizeof(*local_addr))))
		perror("bind udp socket");
	return ret;
}

static struct session_net *
init_udp_socket(struct endpoint_info *local_ei, struct endpoint_info *remote_ei)
{
	struct session_net *ses_net;
	struct session_udp_socket *udp_socket;
	int sockfd;
	struct sockaddr_in remote_addr, local_addr;

	ses_net = malloc(sizeof(struct session_net));
	if (!ses_net) {
		free(ses_net);
		return NULL;
	}

	udp_socket = malloc(sizeof(struct session_udp_socket));
	if (!udp_socket) {
		free(udp_socket);
		return NULL;
	}
	ses_net->raw_net_private = udp_socket;

	sockfd = udp_socket_open();
	if(sockfd < 0) {
		printf("Fail to open udp socket.\n");
		goto free;
	}

	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(remote_ei->udp_port);
	remote_addr.sin_addr.s_addr = htonl(remote_ei->ip);

	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(local_ei->udp_port);
	local_addr.sin_addr.s_addr = htonl(local_ei->ip);

	if (udp_socket_bind(sockfd, &local_addr) < 0) {
		printf("Fail to bind socket to port %d, errorno: %d\n",
		       local_ei->udp_port, errno);
		goto free;
	}

	udp_socket->sockfd = sockfd;
	udp_socket->remote_addr = remote_addr;

	return ses_net;

free:
	free(ses_net);
	free(udp_socket);
	return NULL;
}

struct raw_net_ops udp_socket_ops = {
	.name			= "udp_socket",
	.send_one		= udp_socket_send,
	.receive_one		= udp_socket_receive,
	.receive_one_zerocopy	= NULL,
	.init			= init_udp_socket,
}; 
