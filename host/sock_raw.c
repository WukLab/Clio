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

#include <uapi/compiler.h>

/*
 * Open a raw socket file descriptor.
 * It will return the socket fd, and fill the parameters.
 */
int raw_socket_open(char *if_name, struct ifreq *if_idx, struct ifreq *if_mac)
{
	int sockfd;

	sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	/* Get the index of the interface to send on */
	memset(if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx->ifr_name, if_name, IFNAMSIZ);
	if (ioctl(sockfd, SIOCGIFINDEX, if_idx) < 0) {
		perror("SIOCGIFINDEX");
		return -1;
	}

	/* Get the MAC address of the interface to send on */
	memset(if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac->ifr_name, if_name, IFNAMSIZ);
	if (ioctl(sockfd, SIOCGIFHWADDR, if_mac) < 0) {
		perror("SIOCGIFHWADDR");
		return -1;
	}
	return sockfd;
}

/*
 * Return the number of bytes that was sent out.
 */
int raw_socket_send(int sock_fd, char *buf, int buf_size, struct sockaddr_ll *saddr)
{
	int ret;

	ret = sendto(sock_fd, buf, buf_size, 0, (struct sockaddr *)saddr, sizeof(*saddr));
	if (unlikely(ret < buf_size)) {
		perror("sendto");
		printf("WARN: msg is chunked. Original: %d Actual: %d\n",
			buf_size, ret);
	}
	return ret;
}

void test_raw_socket()
{
	int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;

	void *buf;
	int buf_size;
	ssize_t ret;

	struct ether_header *eh;
	struct iphdr *iph;
	struct sockaddr_ll saddr;

	sockfd = raw_socket_open("eno1", &if_idx, &if_mac);
	if (sockfd < 0) {
		printf("Fail to open raw socket.\n");
		return;
	}

	buf_size = 1024;
	buf = malloc(buf_size);
	if (!buf)
		return;
	memset(buf, 0, buf_size);

	eh = (struct ether_header *)buf;
	iph = (struct iphdr *)(eh + sizeof(struct ether_header));

	/*
	 * Prepare Ethernet Headers
	 * Source MAC + Dest MAC + Type
	 */
#define MY_DEST_MAC0	0x01
#define MY_DEST_MAC1	0x02
#define MY_DEST_MAC2	0x03
#define MY_DEST_MAC3	0x04
#define MY_DEST_MAC4	0x05
#define MY_DEST_MAC5	0x06
	memcpy(eh->ether_shost, &if_mac.ifr_hwaddr.sa_data, 6);
	eh->ether_dhost[0] = MY_DEST_MAC0;
	eh->ether_dhost[1] = MY_DEST_MAC1;
	eh->ether_dhost[2] = MY_DEST_MAC2;
	eh->ether_dhost[3] = MY_DEST_MAC3;
	eh->ether_dhost[4] = MY_DEST_MAC4;
	eh->ether_dhost[5] = MY_DEST_MAC5;
	eh->ether_type = htons(ETH_P_IP);

	/* Index of the network device */
	saddr.sll_ifindex = if_idx.ifr_ifindex;
	saddr.sll_halen = ETH_ALEN;
	saddr.sll_addr[0] = MY_DEST_MAC0;
	saddr.sll_addr[1] = MY_DEST_MAC1;
	saddr.sll_addr[2] = MY_DEST_MAC2;
	saddr.sll_addr[3] = MY_DEST_MAC3;
	saddr.sll_addr[4] = MY_DEST_MAC4;
	saddr.sll_addr[5] = MY_DEST_MAC5;

	/*
	 * Before sending it out, you can cook its IP, UDP, and Lego
	 * headers and payload.
	 */

	ret = raw_socket_send(sockfd, buf, buf_size, &saddr);
	if (ret != buf_size) {
		printf("Send has issues: %d %d\n", ret, buf_size);
	}
}
