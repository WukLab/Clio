#ifndef _LEGOMEM_UAPI_NET_SESSION_H_
#define _LEGOMEM_UAPI_NET_SESSION_H_

#include <errno.h>
#include <stdio.h>
#include <uapi/net_header.h>
#include <uapi/hashtable.h>

/*
 * This structure describes a specific network connection
 * between an application and the LegoMem board.
 */
struct session_net {
	unsigned int		board_ip;
	unsigned int		session_id;

	/*
	 * Session is by default a per-thread object.
	 * Thus it belongs to a certain thread, or tid.
	 */
	pid_t			tid;

	/* The board this session belongs to */
	struct board_info	*board_info;

	/* The endpoint info of this sepcific network session */
	struct endpoint_info	local_ei, remote_ei;

	/* The Ethernet/IP/UDP header info, 44 bytes */
	struct routing_info	route;

	struct hlist_node	ht_link_host;
	struct hlist_node	ht_link_board;
	struct hlist_node	ht_link_context;

	/* Private data used by transport layer */
	void			*transport_private;

	/* Private data used by raw net layer */
	void 			*raw_net_private;
};

struct transport_net_ops {
	char *name;

	int (*init_once)(struct endpoint_info *);
	void (*exit)(void);

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

	int (*open_session)(struct session_net *, struct endpoint_info *, struct endpoint_info *);
	int (*close_session)(struct session_net *);
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

	int (*init_once)(struct endpoint_info *);
	void (*exit)(void);

	/* Send one packet */
	int (*send_one)(struct session_net *, void *, size_t);

	/*
	 * Receive one packet
	 * Blocking call, return when there is packet.
	 */
	int (*receive_one)(void *, size_t);

	/*
	 * The callee will return the pointer to the buffer and buf size.
	 * This function will not do any copy.
	 */
	int (*receive_one_zerocopy)(void **, size_t *);

	/*
	 * Receive one packet
	 * Non-blocking call, return immediately.
	 */
	int (*receive_one_nb)(void *, size_t);

	int (*open_session)(struct session_net *, struct endpoint_info *, struct endpoint_info *);
	int (*close_session)(struct session_net *);

};

extern struct raw_net_ops raw_verbs_ops;
extern struct raw_net_ops raw_socket_ops;
extern struct raw_net_ops udp_socket_ops;
extern struct transport_net_ops transport_bypass_ops;
extern struct transport_net_ops transport_gbn_ops;

extern struct raw_net_ops *raw_net_ops;
extern struct transport_net_ops *transport_net_ops;

static inline int
raw_net_send(struct session_net *net, void *buf, size_t buf_size)
{
	return raw_net_ops->send_one(net, buf, buf_size);
}

static inline int
raw_net_receive(void *buf, size_t buf_size)
{
	return raw_net_ops->receive_one(buf, buf_size);
}

static inline int
raw_net_receive_zerocopy(void **buf, size_t *buf_size)
{
	if (likely(raw_net_ops->receive_one_zerocopy))
		return raw_net_ops->receive_one_zerocopy(buf, buf_size);
	return -ENOSYS;
}

static inline int
raw_net_receive_nb(void *buf, size_t buf_size)
{
	if (likely(raw_net_ops->receive_one_nb))
		return raw_net_ops->receive_one_nb(buf, buf_size);
	return -ENOSYS;
}

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

/*
 * Cook the L2-L4 headers.
 * @len: the length of the whole packet.
 */
static __always_inline int
prepare_headers(struct routing_info *route, void *buf, unsigned int len)
{
	struct routing_info *ri;

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

#define LEGOMEM_MGMT_SESSION_ID	(0)

/*
 * Check if a certain session is node local management session.
 */
static inline bool test_management_session(struct session_net *ses)
{
	if (ses->session_id == LEGOMEM_MGMT_SESSION_ID)
		return true;
	return false;
}

#endif /* _LEGOMEM_UAPI_NET_SESSION_H_ */
