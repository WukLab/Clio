/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>

#include "net.h"

int sysctl_link_mtu = 1500;

struct raw_net_ops *raw_net_ops;
struct transport_net_ops *transport_net_ops;

static inline int
net_send(struct session_net *net, void *buf, size_t buf_size)
{
	return transport_net_ops->send_one(net, buf, buf_size);
}

static inline int
net_receive(struct session_net *net, void *buf, size_t buf_size)
{
	return transport_net_ops->receive_one(net, buf, buf_size);
}

/*
 * Non-blocking receive. It will return immediately if there is no message upon
 * calling. It's an optional interface provided by transport layer.
 */
static inline int
net_receive_nb(struct session_net *net, void *buf, size_t buf_size)
{
	if (transport_net_ops->receive_one_nb)
		return transport_net_ops->receive_one_nb(net, buf, buf_size);
	return 0;
}

void dump_packet_headers(void *packet)
{
	struct eth_hdr *eth;
	struct ipv4_hdr *ipv4;
	struct udp_hdr *udp;
	int j;

	if (unlikely(!packet))
		return;

	eth = packet;
	ipv4 = packet + sizeof(*eth);
	udp = packet + sizeof(*eth) + sizeof(*ipv4);

	for (j = 0; j < 6; j++) {
		printf("%x:", eth->src_mac[j]);
	}
	printf(" -> ");
	for (j = 0; j < 6; j++) {
		printf("%x:", eth->dst_mac[j]);
	}

	printf("  IP %x -> %x", ntohl(ipv4->src_ip), ntohl(ipv4->dst_ip));
	printf("  Port %u -> %u\n", ntohs(udp->src_port), ntohs(udp->dst_port));
}

void test_net(struct session_net *ses)
{
	void *send_buf;
	void *recv_buf;
	int buf_size, i, ret;
	bool server;

	struct dummy_payload {
		unsigned long mark;
	};
	struct dummy_payload *payload;

	buf_size = 256;
	send_buf = malloc(buf_size);
	recv_buf = malloc(buf_size);
	if (!send_buf || !recv_buf)
		return;
	memset(send_buf, 0, buf_size);
	memset(recv_buf, 0, buf_size);

	i = 0;

	/*
	 * Please tune this during testing.
	 * One side is server, another is client.
	 */
	server = false;
	if (server) {
		/* Server, recv msg */
		while (1) {
			ret = net_receive(ses, recv_buf, buf_size);
			if (ret <= 0) {
				printf("receive error\n");
				return;
			}

			dump_packet_headers(recv_buf);
			payload = recv_buf + 44;
			printf("Msg %d Payload mark: %lu\n", i, payload->mark);
			i++;
		}
	} else {
		/* Client, send msg */
		while (1) {
			payload = send_buf + 44;
			payload->mark = i++;

			printf("send %d\n", i-1);
			ret = net_send(ses, send_buf, buf_size);
			if (ret <= 0) {
				printf("send error\n");
				return;
			}

			if (i >= 100)
				break;
		}
	}
}

/* TODO more automatic or use XML file */
struct endpoint_info ei_wuklab02 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
	.ip		= 0xc0a80102, /* 192.168.1.2 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab05 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xe4, 0x81, 0x51 },
	.ip		= 0xc0a80105, /* 192.168.1.5 */
	.udp_port	= 8888,
};
struct endpoint_info board_0 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
	.ip		= 0xc0a801c8, /* 192.168.1.200 */
	.udp_port	= 1234,
};

struct session_net *init_net(void)
{
	struct session_net *ses;
	struct endpoint_info *local_ei, *remote_ei;

	/*
	 * XXX
	 * Knobs
	 */
	local_ei = &ei_wuklab02;
	remote_ei = &ei_wuklab05;

	//raw_net_ops = &raw_verbs_ops;
	raw_net_ops = &raw_socket_ops;
	printf("Raw Net Layer: using %s\n", raw_net_ops->name);

	ses = raw_net_ops->init(local_ei, remote_ei);
	if (!ses) {
		printf("Fail to create net session\n");
		return NULL;
	}

	//transport_net_ops = &transport_gbn_ops;
	transport_net_ops = &transport_bypass_ops;

	test_net(ses);
	return ses;
}
