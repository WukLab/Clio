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
#include "../core.h"

int sysctl_link_mtu = 1500;

struct raw_net_ops *raw_net_ops = &raw_verbs_ops;
struct transport_net_ops *transport_net_ops = &transport_gbn_ops;

/*
 * Create a session between @local_ei and @remote_ei.
 * Only data structures are updated.
 */
struct session_net *net_open_session(struct endpoint_info *local_ei,
				     struct endpoint_info *remote_ei)
{
	struct session_net *ses;
	int ret;

	ses = alloc_session();
	if (!ses)
		return NULL;

	/*
	 * Open raw net layer before open transport layer
	 * as the latter one might start calling raw net APIs right away
	 */
	ret = raw_net_ops->open_session(ses, local_ei, remote_ei);
	if (ret)
		goto free;

	ret = transport_net_ops->open_session(ses, local_ei, remote_ei);
	if (ret)
		goto close_raw;	

	/* Prepare the session info */
	prepare_routing_info(&ses->route, local_ei, remote_ei);
	memcpy(&ses->local_ei, local_ei, sizeof(*local_ei));
	memcpy(&ses->remote_ei, remote_ei, sizeof(*remote_ei));

	return ses;

close_raw:
	raw_net_ops->close_session(ses);
free:
	free_session(ses);
	return NULL;
}

/*
 * Close a network session, free all associated memory.
 * This is an internal function call, thus @ses should not never be NULL.
 */
int net_close_session(struct session_net *ses)
{
	BUG_ON(!ses);

	transport_net_ops->close_session(ses);
	raw_net_ops->close_session(ses);
	free_session(ses);

	return 0;
}

static pthread_spinlock_t dump_lock;

void __dump_packet_headers(void *packet, char *str_buf)
{
	struct eth_hdr *eth;
	struct ipv4_hdr *ipv4;
	struct udp_hdr *udp;
	struct gbn_header *gbn;
	struct lego_header *lego;
	int i;
	char src_ip[INET_ADDRSTRLEN];
	char dst_ip[INET_ADDRSTRLEN];
	struct in_addr src_addr, dst_addr;

#define DUMP_PR(fmt, ...)						\
	do {								\
		if (str_buf) {						\
			int ret;					\
			ret = sprintf(str_buf, fmt, __VA_ARGS__);	\
			str_buf += ret;					\
		} else							\
			printf(fmt, __VA_ARGS__);			\
	} while (0)

	eth = packet;
	ipv4 = packet + sizeof(*eth);
	udp = packet + sizeof(*eth) + sizeof(*ipv4);
	gbn = to_gbn_header(packet);
	lego = to_lego_header(packet);

	for (i = 0; i < 6; i++) {
		if (i < 5)
			DUMP_PR("%x:", eth->src_mac[i]);
		else
			DUMP_PR("%x->", eth->src_mac[i]);
	}

	for (i = 0; i < 6; i++) {
		if (i < 5)
			DUMP_PR("%x:", eth->dst_mac[i]);
		else
			DUMP_PR("%x ", eth->dst_mac[i]);
	}

	/*
	 * The packet handed over should be in network order
	 * Handle it properly.
	 */
	src_addr.s_addr = ipv4->src_ip;
	dst_addr.s_addr = ipv4->dst_ip;
	inet_ntop(AF_INET, &src_addr, src_ip, sizeof(src_ip));
	inet_ntop(AF_INET, &dst_addr, dst_ip, sizeof(dst_ip));

#ifdef CONFIG_TRANSPORT_GBN
	DUMP_PR("ip:port:gbn %s:%u:%u->%s:%u:%u (%s) ",
		src_ip, ntohs(udp->src_port), get_gbn_src_session(gbn),
		dst_ip, ntohs(udp->dst_port), get_gbn_dst_session(gbn),
		gbn_pkt_type_str(gbn->type));
#endif

	DUMP_PR("lego pid=%u tag=%#x opcode=%#x sesid=%u (%s)",
		lego->pid, lego->tag, lego->opcode, lego->src_sesid,
		legomem_opcode_str(lego->opcode));

	if (!str_buf)
		printf("\n");
}

void dump_packet_headers(void *packet, char *str_buf)
{
	if (unlikely(!packet))
		return;

	/*
	 * Caller is taking a dump anyway,
	 * spin wait is okay.
	 */
	pthread_spin_lock(&dump_lock);
	__dump_packet_headers(packet, str_buf);
	pthread_spin_unlock(&dump_lock);
}

/*
 * Initialize transport and raw network layer
 * Called once during startup.
 */
int init_net(struct endpoint_info *local_ei)
{
	int ret;

	pthread_spin_init(&dump_lock, PTHREAD_PROCESS_PRIVATE);

	sysctl_link_mtu = get_device_mtu(global_net_dev);
	dprintf_INFO("%s MTU: %d\n", global_net_dev, sysctl_link_mtu);

	dprintf_INFO("Raw Net Layer: using %s\n", raw_net_ops->name);
	dprintf_INFO("Transport Layer: using %s\n", transport_net_ops->name);

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
