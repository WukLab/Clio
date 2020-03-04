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

struct dummy_payload {
	unsigned long mark;
};

void dump_packet_headers(void *packet)
{
	struct eth_hdr *eth;
	struct ipv4_hdr *ipv4;
	struct udp_hdr *udp;
	struct gbn_header *gbn;
	struct dummy_payload *payload;
	int j;

	if (unlikely(!packet))
		return;

	eth = packet;
	ipv4 = packet + sizeof(*eth);
	udp = packet + sizeof(*eth) + sizeof(*ipv4);
	gbn = packet + GBN_HEADER_OFFSET;
	payload = packet + LEGO_HEADER_OFFSET;

	for (j = 0; j < 6; j++) {
		printf("%x:", eth->src_mac[j]);
	}
	printf(" -> ");
	for (j = 0; j < 6; j++) {
		printf("%x:", eth->dst_mac[j]);
	}

	printf("  IP %x -> %x", ntohl(ipv4->src_ip), ntohl(ipv4->dst_ip));
	printf("  Port %u -> %u", ntohs(udp->src_port), ntohs(udp->dst_port));
	printf("  Seqnum %u, Mark %lu\n", gbn->seqnum, payload->mark);
}

void* send_msg(void *arg)
{
#define NR_SEND_BUF_SLOTS	(256)
#define NR_MSG_PER_THREAD	(100)
	void *send_buf;
	struct dummy_payload *send_buf_ring[NR_SEND_BUF_SLOTS];
	int buf_size, i, ret;
	struct dummy_payload *payload;
	struct session_net *ses;

	buf_size = 256;
	ses = (struct session_net *)arg;

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++) {
		if (!(send_buf_ring[i] = malloc(buf_size)))
			return NULL;
		memset(send_buf_ring[i], 0, buf_size);
	}

	i = 0;

	while (1) {
		send_buf = send_buf_ring[i % NR_SEND_BUF_SLOTS];
		payload = send_buf + LEGO_HEADER_OFFSET;
		payload->mark = i++;

		printf("send %d\n", i - 1);
		ret = net_send(ses, send_buf, buf_size);
		if (ret <= 0) {
			printf("send error\n");
			return NULL;
		}

		if (i >= NR_MSG_PER_THREAD)
			break;
	}

	for (i = 0; i < NR_SEND_BUF_SLOTS; i++)
		free(send_buf_ring[i]);
	
	return NULL;
}

void test_net(struct session_net *ses)
{
#define NR_TEST_SEND_THREAD	(5)
	void *recv_buf;
	int buf_size, i, ret;
	bool server;
	struct dummy_payload *payload;
	struct gbn_header *hdr;
	pthread_t send_thread[NR_TEST_SEND_THREAD];

	buf_size = 256;

	recv_buf = malloc(buf_size);
	if (!recv_buf)
		return;
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

			hdr = recv_buf + GBN_HEADER_OFFSET;
			payload = recv_buf + LEGO_HEADER_OFFSET;
			printf("Msg %d Payload mark: %lu\n", i, payload->mark);
			/* seqnum starts from 1 */
			if (hdr->seqnum != i + 1) {
				printf("Receive out of order. Expected seq#: %d, Received seq#: %d\n",
				       i + 1, hdr->seqnum);
			}
			i++;
		}
	} else {
		/* Client, send msg */
		for (i = 0; i < NR_TEST_SEND_THREAD; i++)
			pthread_create(&send_thread[i], NULL, send_msg, ses);
	}

	for (i = 0; i < NR_TEST_SEND_THREAD; i++)
		pthread_join(send_thread[i], NULL);

	sleep(3);

	free(recv_buf);
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
struct endpoint_info ei_wuklab06 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb3, 0x54, 0x11 },
	.ip		= 0xc0a80106, /* 192.168.1.6 */
	.udp_port	= 8888,
};
struct endpoint_info board_0 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
	.ip		= 0xc0a801c8, /* 192.168.1.200 */
	.udp_port	= 1234,
};
struct endpoint_info board_1 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0x00, 0x00 },
	.ip		= 0xc0a80180, /* 192.168.1.128 */
	.udp_port	= 1234,
};

/* TODO */
struct session_net *tmp_global_session;
struct session_net *find_session(void *packet)
{
	return tmp_global_session;
}

struct session_net *init_net(void)
{
	struct session_net *ses;
	struct endpoint_info *local_ei, *remote_ei;
	int ret;

	/*
	 * XXX
	 * Knobs
	 */
	local_ei = &ei_wuklab02;
	remote_ei = &ei_wuklab05;

	raw_net_ops = &raw_verbs_ops;
	//raw_net_ops = &raw_socket_ops;
	//raw_net_ops = &udp_socket_ops;
	printf("Raw Net Layer: using %s\n", raw_net_ops->name);

	ses = raw_net_ops->init(local_ei, remote_ei);
	if (!ses) {
		printf("Fail to init raw net session\n");
		return NULL;
	}

	transport_net_ops = &transport_gbn_ops;
	//transport_net_ops = &transport_bypass_ops;
	printf("Transport Layer: using %s\n", transport_net_ops->name);

	ret = transport_net_ops->init(ses);
	if (ret) {
		printf("Fail to init transport session\n");
		return NULL;
	}

	tmp_global_session = ses;

	test_net(ses);
	return ses;
}
