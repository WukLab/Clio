/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

#include <uapi/net_header.h>

/*
 * Open a raw socket file descriptor.
 * It will return the socket fd, and fill the parameters.
 *
 * Return positive sockfd on success, otherwise negtive error values.
 */
int raw_socket_open(char *if_name, struct ifreq *if_idx, struct ifreq *if_mac)
{
	int sockfd;
	int ret;

	if (unlikely(!if_name || !if_idx || !if_mac))
		return -EINVAL;

	//sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
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
		perror("SIOCGIFINDEX");
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

/*
 * Return the number of bytes that was sent out.
 */
int raw_socket_send(int sock_fd, char *buf, size_t buf_size, struct sockaddr_ll *saddr)
{
	int ret;

	if (unlikely(!buf))
		return -EINVAL;

	ret = sendto(sock_fd, buf, buf_size, 0, (struct sockaddr *)saddr, sizeof(*saddr));
	if (unlikely(ret < buf_size)) {
		perror("sendto");
		printf("WARN: msg is chunked. Original: %d Actual: %d\n",
			buf_size, ret);
	}
	return ret;
}

int raw_socket_receive(int sock_fd, char *buf, size_t buf_size)
{
	if (unlikely(!buf))
		return -EINVAL;

	return recvfrom(sock_fd, buf, buf_size, 0, NULL, NULL);
}

/*
 * This function is used to demonstrate the
 * usages of raw socket and for testing purpose.
 */
int test_raw_socket(void)
{
	int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;

	void *send_buf;
	void *recv_buf;
	int buf_size;
	ssize_t ret;

	struct ether_header *eh;
	struct iphdr *iph;
	struct sockaddr_ll saddr;

	sockfd = raw_socket_open("p4p1", &if_idx, &if_mac);
	if (sockfd < 0) {
		printf("Fail to open raw socket.\n");
		return;
	}

	/*
	 * Create two temporary buffers
	 * for sending and receiving.
	 */
	buf_size = 128;
	send_buf = malloc(buf_size);
	recv_buf = malloc(buf_size);
	if (!send_buf || !recv_buf)
		return;
	memset(send_buf, 0, buf_size);
	memset(recv_buf, 0, buf_size);

	struct endpoint_info ei_wuklab02 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
		.ip		= 0xc0a80102, /* 192.168.1.2 */
		.udp_port	= 8888,
	};
	struct endpoint_info board_0 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
		.ip		= 0xc0a801c8, /* 192.168.1.200 */
		.udp_port	= 1234,
	};
	struct routing_info board_ri, *ri;
	struct endpoint_info *local_ei, *remote_ei;
	local_ei	= &ei_wuklab02;
	remote_ei	= &board_0;

	/* Cook Eth IP UDP headers */
	prepare_routing_info(&board_ri, local_ei, remote_ei);
	memcpy(send_buf, &board_ri, sizeof(board_ri));
	ri = (struct routing_info *)send_buf;
	ri->ipv4.tot_len = htons(buf_size - sizeof(struct eth_hdr));
	ri->ipv4.check = 0;
	ri->ipv4.check = ip_csum(&ri->ipv4, 5);

	ri->udp.len = htons(buf_size - sizeof(struct eth_hdr) - sizeof(struct ipv4_hdr));

	saddr.sll_ifindex = if_idx.ifr_ifindex;
	saddr.sll_halen = ETH_ALEN;
	memcpy(saddr.sll_addr, local_ei->mac, 6);

	int i, nr_msg;

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
		raw_socket_send(sockfd, send_buf, buf_size, &saddr);

		memset(recv_buf, 0, buf_size);
		ret = raw_socket_receive(sockfd, recv_buf, buf_size);

		dp = recv_buf + 44;
		printf("i %d ret: %d mark=%d\n", i, ret, dp->mark);
		struct eth_hdr *hdr = recv_buf;
		int j;
		for (j = 0; j < 6; j++) {
			printf("%x:", hdr->src_mac[j]);
		}
		printf(" -> ");
		for (j = 0; j < 6; j++) {
			printf("%x:", hdr->dst_mac[j]);
		}
		printf("\n\n");
	}
}
