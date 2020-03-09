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
