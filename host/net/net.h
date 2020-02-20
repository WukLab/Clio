/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _HOST_NET_NET_H_
#define _HOST_NET_NET_H_

#include <errno.h>
#include <stdio.h>
#include <uapi/net_header.h>

extern int sysctl_link_mtu;

struct session_net {
	struct endpoint_info	local_ei, remote_ei;
	struct routing_info	route;
	void 			*transport_private;
};

struct session_net *init_net(void);
void dump_packet_headers(void *packet);

struct transport_net_ops {
	char *name;

	/* Send one packet */
	int (*send_one)(struct session_net *, void *, size_t);

	/*
	 * Receive one packet
	 * Blocking call, return when there is packet.
	 */
	int (*receive_one)(struct session_net *, void *, size_t);

	/*
	 * Receive one packet
	 * Non-blocking call, return immediately.
	 */
	int (*receive_one_nb)(struct session_net *, void *, size_t);

	int (*init)(void);
};

/*
 * The lowest net transport layer, used to send/receive
 * packets to/from the network device.
 *
 * In theory, this ops can support the following:
 * - Raw Socket
 * - Raw IB verbs
 * - DPDK
 */
struct raw_net_ops {
	char *name;

	/* Send one packet */
	int (*send_one)(struct session_net *, void *, size_t);

	/*
	 * Receive one packet
	 * Blocking call, return when there is packet.
	 */
	int (*receive_one)(struct session_net *, void *, size_t);

	/*
	 * Receive one packet
	 * Non-blocking call, return immediately.
	 */
	int (*receive_one_nb)(struct session_net *, void *, size_t);

	/* Init this layer */
	struct session_net *(*init)(struct endpoint_info *, struct endpoint_info *);
};

extern struct raw_net_ops raw_verbs_ops;
extern struct raw_net_ops raw_socket_ops;

extern struct transport_net_ops transport_bypass_ops;

extern struct raw_net_ops *raw_net_ops;
extern struct transport_net_ops *transport_net_ops;

static inline int
raw_net_send(struct session_net *net, void *buf, size_t buf_size)
{
	return raw_net_ops->send_one(net, buf, buf_size);
}

static inline int
raw_net_receive(struct session_net *net, void *buf, size_t buf_size)
{
	return raw_net_ops->receive_one(net, buf, buf_size);
}

static inline int
raw_net_receive_nb(struct session_net *net, void *buf, size_t buf_size)
{
	if (raw_net_ops->receive_one_nb)
		return raw_net_ops->receive_one_nb(net, buf, buf_size);
	return 0;
}

/*
 * Cook the L2-L4 headers.
 * @len: the length of the whole packet.
 */
static __always_inline int
prepare_headers(struct routing_info *route, void *buf, unsigned int len)
{
	struct routing_info *ri;

	if (unlikely(!route || !buf))
		return -EINVAL;

	if (unlikely(len > sysctl_link_mtu)) {
		printf("WARNING: Max MTU: %d. Length: %d\n",
			sysctl_link_mtu, len);
		return -EINVAL;
	}

	/*
	 * Directly copy the provided routing info
	 * into the packet buffer
	 */
	memcpy(buf, route, sizeof(struct routing_info));

	ri = (struct routing_info *)buf;
	ri->ipv4.tot_len = htons(len - sizeof(struct eth_hdr));
	ri->ipv4.check = 0;
	ri->ipv4.check = ip_csum(&ri->ipv4, ri->ipv4.ihl);
	ri->udp.len = htons(len - sizeof(struct eth_hdr) - sizeof(struct ipv4_hdr));

	return 0;
}

#endif /* _HOST_NET_NET_H_ */
