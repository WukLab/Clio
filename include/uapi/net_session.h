#ifndef _LEGOMEM_UAPI_NET_SESSION_H_
#define _LEGOMEM_UAPI_NET_SESSION_H_

#include <errno.h>
#include <stdio.h>
#include <uapi/net_header.h>
#include <uapi/hashtable.h>

#define SESSION_NET_FLAGS_ALLOCATED	(0x1)

/*
 * This structure describes a specific network connection
 * between an application and the LegoMem board.
 */
struct session_net {
	unsigned int		board_ip;
	unsigned int		udp_port;

	unsigned int		session_id;
	unsigned int		remote_session_id;

	unsigned long		flags;

	pthread_t		thread;

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

static inline void set_local_session_id(struct session_net *ses, unsigned int id)
{
	ses->session_id = id;
}

static inline void set_remote_session_id(struct session_net *ses, unsigned int id)
{
	ses->remote_session_id = id;
}

static inline unsigned int
get_local_session_id(struct session_net *ses)
{
	return ses->session_id;
}

static inline unsigned int
get_remote_session_id(struct session_net *ses)
{
	return ses->remote_session_id;
}

struct transport_net_ops {
	char *name;

	int (*init_once)(struct endpoint_info *);
	void (*exit)(void);

	/* Send one packet */
	int (*send_one)(struct session_net *, void *, size_t, void *);

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

	int (*reg_send_buf)(struct session_net *, void *buf, size_t buf_size);
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
	int (*send_one)(struct session_net *, void *, size_t, void *);

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

	int (*reg_send_buf)(struct session_net *, void *buf, size_t buf_size);
};

extern struct raw_net_ops raw_verbs_ops;
extern struct raw_net_ops raw_socket_ops;
extern struct raw_net_ops raw_udp_socket_ops;
extern struct transport_net_ops transport_bypass_ops;
extern struct transport_net_ops transport_gbn_ops;

extern struct raw_net_ops *raw_net_ops;
extern struct transport_net_ops *transport_net_ops;

static inline int
raw_net_reg_send_buf(struct session_net *ses, void *buf, size_t buf_size)
{
	if (raw_net_ops->reg_send_buf)
		return raw_net_ops->reg_send_buf(ses, buf, buf_size);
	return 0;
}

/*
 * From raw network layer's pespective, this no such concept
 * as session, which is a transport layer concept.
 * We pass down the @net because we have saved the private
 * data inside that one.
 */
static inline int
raw_net_send(struct session_net *net, void *buf, size_t buf_size, void *route)
{
	return raw_net_ops->send_one(net, buf, buf_size, route);
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
default_transport_reg_send_buf(struct session_net *net, void *buf, size_t buf_size)
{
	return raw_net_reg_send_buf(net, buf, buf_size);
}

/*
 * Register a per-session send_buf
 * This is necessary for verbs-based users.
 *
 * 1. Each session can only have one registered send_buf.
 * 2. Do not use a stack, use malloc'ed region.
 */
static inline int
net_reg_send_buf(struct session_net *ses, void *buf, size_t buf_size)
{
	return transport_net_ops->reg_send_buf(ses, buf, buf_size);
}

static inline int
net_send_with_route(struct session_net *net, void *buf, size_t buf_size, void *route)
{
	return transport_net_ops->send_one(net, buf, buf_size, route);
}

static inline int
net_send(struct session_net *net, void *buf, size_t buf_size)
{
	return transport_net_ops->send_one(net, buf, buf_size, NULL);
}

static inline int
net_receive(struct session_net *net, void *buf, size_t buf_size)
{
	return transport_net_ops->receive_one(net, buf, buf_size);
}

static inline int
net_send_and_receive(struct session_net *net,
		     void *tx_buf, size_t tx_buf_size,
		     void *rx_buf, size_t rx_buf_size)
{
	int ret;

	ret = net_send(net, tx_buf, tx_buf_size);
	if (ret <= 0)
		return ret;

	return net_receive(net, rx_buf, rx_buf_size);
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
 * Check if a certain session involves mgmt session at either end
 */
static inline bool test_management_session(struct session_net *ses)
{
	if (get_local_session_id(ses) == LEGOMEM_MGMT_SESSION_ID)
		return true;
	if (get_remote_session_id(ses) == LEGOMEM_MGMT_SESSION_ID)
		return true;
	return false;
}

#endif /* _LEGOMEM_UAPI_NET_SESSION_H_ */
