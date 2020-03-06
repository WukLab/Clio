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

/*
 * Create a session between @local_ei and @remote_ei.
 * Only data structures are updated.
 */
struct session_net *net_open_session(struct endpoint_info *local_ei,
				     struct endpoint_info *remote_ei)
{
	struct session_net *ses;
	int ret;

	ses = malloc(sizeof(struct session_net));
	if (!ses)
		return NULL;
	memset(ses, 0, sizeof(*ses));

	ret = transport_net_ops->open_session(ses, local_ei, remote_ei);
	if (ret)
		goto free;	

	ret = raw_net_ops->open_session(ses, local_ei, remote_ei);
	if (ret)
		goto close_transport;

	/* Prepare the session info */
	prepare_routing_info(&ses->route, local_ei, remote_ei);
	memcpy(&ses->local_ei, local_ei, sizeof(*local_ei));
	memcpy(&ses->remote_ei, remote_ei, sizeof(*remote_ei));

	return ses;

close_transport:
	transport_net_ops->close_session(ses);
free:
	free(ses);
	return NULL;
}

/*
 * Close a network session, free all associated memory.
 */
int net_close_session(struct session_net *ses)
{
	if (!ses)
		return -EINVAL;

	transport_net_ops->close_session(ses);
	raw_net_ops->close_session(ses);
	free(ses);

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

void *send_msg(void *arg)
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

/*
 * Initialize transport and raw network layer
 * Called once during startup.
 */
int init_net(struct endpoint_info *local_ei)
{
	int ret;

	//raw_net_ops = &raw_verbs_ops;
	//raw_net_ops = &raw_socket_ops;
	raw_net_ops = &udp_socket_ops;
	printf("%s(): Raw Net Layer: using %s\n", __func__, raw_net_ops->name);

	transport_net_ops = &transport_gbn_ops;
	//transport_net_ops = &transport_bypass_ops;
	printf("%s(): Transport Layer: using %s\n", __func__, transport_net_ops->name);

	ret = raw_net_ops->init_once(local_ei);
	if (ret) {
		printf("Fail to init raw net layer\n");
		return ret;
	}

	ret = transport_net_ops->init_once(local_ei);
	if (ret) {
		printf("Fail to init transport session\n");

		raw_net_ops->exit();
		return ret;
	}
	return 0;
}
