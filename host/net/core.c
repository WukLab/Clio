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

/*
 * This is the local endpoint info
 * Constructed during startup based on network device and UDP port used.
 */
struct endpoint_info default_local_ei;

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

static pthread_spinlock_t dump_lock;

static void __dump_packet_headers(void *packet, char *str_buf)
{
	struct eth_hdr *eth;
	struct ipv4_hdr *ipv4;
	struct udp_hdr *udp;
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

	DUMP_PR("ip:port %s:%u->%s:%u",
		src_ip, ntohs(udp->src_port), dst_ip, ntohs(udp->dst_port));
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
