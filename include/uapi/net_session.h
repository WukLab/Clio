/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#ifndef _LEGOMEM_UAPI_NET_SESSION_H_
#define _LEGOMEM_UAPI_NET_SESSION_H_

#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <uapi/net_header.h>
#include <uapi/page.h>
#include <uapi/bitops.h>
#include <uapi/bitmap.h>
#include <stdatomic.h>
#include <uapi/hashtable.h>

#define SESSION_NET_FLAGS_ALLOCATED		(0x1)
#define SESSION_NET_FLAGS_THREAD_CREATED	(0x2)

/*
 * This structure describes a specific network connection
 * between an application and the LegoMem board.
 *
 * HACK: if you add anything, do not forget to update init_net_session().
 */
struct session_net {
	unsigned int		board_ip;
	unsigned int		udp_port;

	unsigned int		session_id;
	unsigned int		remote_session_id;

	unsigned long		flags;

	/*
	 * This is the client side polling thread, if any.
	 * Mgmt session's polling thread is created during startup.
	 * User session's polling thread is created when this node
	 * receives a open_session request. See handle_open_session() for details.
	 */
	pthread_t		thread;
	bool			thread_should_stop;

	void			*registered_send_buf;

	/*
	 * Used by threads wishing to use the mgmt session of a board.
	 * We have to do this because all threads share one single mgmt session,
	 * thus there is only one set of buffers, impossible to do RPC.
	 *
	 * You can see this used across api.c
	 */
	pthread_spinlock_t	lock;

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

	/*
	 * We have various hlist_nodes for various hashtables.
	 * Each board_info wants to maintain a list of open sessions
	 * Each user context wants to maintain a list of open sessions
	 * Each vRegion as well
	 */

	/* for board_info, used in sched.h */
	struct hlist_node	ht_link_board;

	/* for legomem_context, used in context.c */
	struct hlist_node	ht_link_context;

	/* for legomem_vregion */
	struct hlist_node	ht_link_vregion;

	/* Private data used by transport layer */
	void			*transport_private;

	/* Private data used by raw net layer */
	void 			*raw_net_private;

	/*
	 * Added for OSDI21
	 */

	/*
	 * Record outstanding reads and writes
	 * Used to implement fence
	 */
	atomic_int		outstanding_reads;
	atomic_int		outstanding_writes;

	/* DECLARE_BITMAP(outstanding_reads_map, NR_VIRTUAL_PAGES); */
	/* DECLARE_BITMAP(outstanding_writes_map, NR_VIRTUAL_PAGES); */
} __aligned(64);

static inline void init_session_net(struct session_net *p)
{
	memset(p, 0, sizeof(*p));
	pthread_spin_init(&p->lock, PTHREAD_PROCESS_PRIVATE);
	INIT_HLIST_NODE(&p->ht_link_board);
	INIT_HLIST_NODE(&p->ht_link_context);
	INIT_HLIST_NODE(&p->ht_link_vregion);
	atomic_store(&p->outstanding_reads, 0);
	atomic_store(&p->outstanding_writes, 0);
	/* bitmap_zero(p->outstanding_reads_map, NR_VIRTUAL_PAGES); */
	/* bitmap_zero(p->outstanding_writes_map, NR_VIRTUAL_PAGES); */
}

static inline bool ses_thread_should_stop(struct session_net *ses)
{
	return READ_ONCE(ses->thread_should_stop);
}

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

struct msg_buf {
	void *buf;
	size_t max_buf_size;
	void *private;
} __aligned(64);

struct transport_net_ops {
	char *name;

	int (*init_once)(struct endpoint_info *);
	void (*exit)(void);

	/* Send one packet */
	int (*send_one)(struct session_net *, void *, size_t, void *);
	int (*send_one_msg_buf)(struct session_net *, struct msg_buf *, size_t, void *);

	/*
	 * Receive one packet
	 * Blocking call, return when there is packet.
	 */
	int (*receive_one)(struct session_net *, void *, size_t);
	int (*receive_one_zerocopy)(struct session_net *, void **, size_t *);
	int (*receive_one_zerocopy_nb)(struct session_net *, void **, size_t *);

	/*
	 * Receive one packet
	 * Non-blocking call, return immediately.
	 */
	int (*receive_one_nb)(struct session_net *, void *, size_t);

	int (*open_session)(struct session_net *, struct endpoint_info *, struct endpoint_info *);
	int (*close_session)(struct session_net *);

	int (*reg_send_buf)(struct session_net *, void *buf, size_t buf_size);

	struct msg_buf *(*reg_msg_buf)(struct session_net *, void *buf, size_t buf_size);
	int (*dereg_msg_buf)(struct session_net *, struct msg_buf *);
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
	int (*send_one_msg_buf)(struct session_net *, struct msg_buf *, size_t, void *);

	/*
	 * Receive one packet
	 * Blocking call, return when there is packet.
	 */
	int (*receive_one)(struct session_net *, void *, size_t);

	/*
	 * The callee will return the pointer to the buffer and buf size.
	 * This function will not do any copy.
	 */
	int (*receive_one_zerocopy)(struct session_net *, void **, size_t *);
	int (*receive_one_zerocopy_batch)(struct session_net *, void **, size_t *);

	/*
	 * Receive one packet
	 * Non-blocking call, return immediately.
	 */
	int (*receive_one_nb)(struct session_net *, void *, size_t);

	int (*open_session)(struct session_net *, struct endpoint_info *, struct endpoint_info *);
	int (*close_session)(struct session_net *);

	/*
	 * TODO
	 * I should really unify send_buf and msg_buf
	 * its really ugly to have these interfaces, it makes things complex.
	 * We should remove reg_send_buf.
	 */
	int (*reg_send_buf)(struct session_net *, void *buf, size_t buf_size);
	struct msg_buf *(*reg_msg_buf)(struct session_net *, void *buf, size_t buf_size);
	int (*dereg_msg_buf)(struct session_net *, struct msg_buf *);
};

extern struct raw_net_ops raw_verbs_ops;
//extern struct raw_net_ops raw_socket_ops;
//extern struct raw_net_ops raw_udp_socket_ops;
extern struct transport_net_ops transport_bypass_ops;
extern struct transport_net_ops transport_gbn_ops;
extern struct transport_net_ops transport_rpc_ops;

extern struct raw_net_ops *raw_net_ops;
extern struct transport_net_ops *transport_net_ops;

static inline int
raw_net_reg_send_buf(struct session_net *ses, void *buf, size_t buf_size)
{
	if (raw_net_ops->reg_send_buf)
		return raw_net_ops->reg_send_buf(ses, buf, buf_size);

	/*
	 * If the underlying raw network layer does not provide this interface,
	 * we assume it does not require it (e.g., no DMA-able memory requirement),
	 * thus return 0 to indicate success.
	 */
	return 0;
}

static inline int
raw_net_dereg_msg_buf(struct session_net *ses, struct msg_buf *mb)
{
	return raw_net_ops->dereg_msg_buf(ses, mb);
}

static inline struct msg_buf *
raw_net_reg_msg_buf(struct session_net *ses, void *buf, size_t buf_size)
{
	return raw_net_ops->reg_msg_buf(ses, buf, buf_size);
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
raw_net_send_msg_buf(struct session_net *net, struct msg_buf *buf, size_t buf_size, void *route)
{
	return raw_net_ops->send_one_msg_buf(net, buf, buf_size, route);
}

static inline int
raw_net_receive(struct session_net *ses, void *buf, size_t buf_size)
{
	return raw_net_ops->receive_one(ses, buf, buf_size);
}

static inline int
raw_net_receive_zerocopy_batch(struct session_net *ses, void **buf, size_t *buf_size)
{
	return raw_net_ops->receive_one_zerocopy_batch(ses, buf, buf_size);
}

static inline int
raw_net_receive_zerocopy(struct session_net *ses, void **buf, size_t *buf_size)
{
	return raw_net_ops->receive_one_zerocopy(ses, buf, buf_size);
}

static inline int
raw_net_receive_nb(struct session_net *ses, void *buf, size_t buf_size)
{
	return raw_net_ops->receive_one_nb(ses, buf, buf_size);
}

static inline int
default_transport_reg_send_buf(struct session_net *net, void *buf, size_t buf_size)
{
	return raw_net_reg_send_buf(net, buf, buf_size);
}

/*
 * Deprecated. DO NOT USE. Use reg_msg_buf.
 *
 * Register a per-session send_buf
 * This is necessary for verbs-based users.
 *
 * 1. Each session can only have one registered send_buf.
 * 2. Do not use a stack, use malloc'ed region.
 */
static __always_inline int
net_reg_send_buf(struct session_net *ses, void *buf, size_t buf_size)
{
	int ret;
	
	ret = transport_net_ops->reg_send_buf(ses, buf, buf_size);
	if (ret)
		return ret;

	ses->registered_send_buf = buf;
	return 0;
}

static __always_inline void *
net_get_send_buf(struct session_net *ses)
{
	return ses->registered_send_buf;
}

static inline int
net_dereg_msg_buf(struct session_net *ses, struct msg_buf *mb)
{
	return raw_net_ops->dereg_msg_buf(ses, mb);
}

static inline struct msg_buf *
net_reg_msg_buf(struct session_net *ses, void *buf, size_t buf_size)
{
	return raw_net_ops->reg_msg_buf(ses, buf, buf_size);
}

/*
 * Not too many places call this function.
 * This one is not intended for normal callers.
 */
static __always_inline int
net_send_with_route(struct session_net *net, void *buf, size_t buf_size, void *route)
{
	return transport_net_ops->send_one(net, buf, buf_size, route);
}

static inline int
net_send_msg_buf(struct session_net *net, struct msg_buf *buf, size_t buf_size)
{
	return transport_net_ops->send_one_msg_buf(net, buf, buf_size, NULL);
}

static __always_inline int
net_send(struct session_net *net, void *buf, size_t buf_size)
{
	return transport_net_ops->send_one(net, buf, buf_size, NULL);
}

static __always_inline int
net_receive_zerocopy(struct session_net *net, void **buf, size_t *buf_size)
{
	return transport_net_ops->receive_one_zerocopy(net, buf, buf_size);
}

static __always_inline int
net_receive_zerocopy_nb(struct session_net *net, void **buf, size_t *buf_size)
{
	return transport_net_ops->receive_one_zerocopy_nb(net, buf, buf_size);
}

static __always_inline int
net_receive(struct session_net *net, void *buf, size_t buf_size)
{
	return transport_net_ops->receive_one(net, buf, buf_size);
}

static __always_inline int
net_send_and_receive(struct session_net *net,
		     void *tx_buf, size_t tx_buf_size,
		     void *rx_buf, size_t rx_buf_size)
{
	net_send(net, tx_buf, tx_buf_size);
	return net_receive(net, rx_buf, rx_buf_size);
}

static __always_inline int
net_send_and_receive_zerocopy(struct session_net *net,
			      void *tx_buf, size_t tx_buf_size,
			      void **rx_buf, size_t *rx_buf_size)
{
	net_send(net, tx_buf, tx_buf_size);
	return net_receive_zerocopy(net, rx_buf, rx_buf_size);
}

/*
 * This is specifically designed for any APIs wishing to
 * use remote board's mgmt session.
 *
 * Because there is only one single set of buffers for a single mgmt session,
 * there is no easy way to support RPC among concurrent threads.
 *
 * Mgmt session is for control path, thus having a sync lock should be ok.
 */
static __always_inline int
net_send_and_receive_lock(struct session_net *net,
			  void *tx_buf, size_t tx_buf_size,
			  void *rx_buf, size_t rx_buf_size)
{
	int ret;

	pthread_spin_lock(&net->lock);
	ret = net_send_and_receive(net, tx_buf, tx_buf_size, rx_buf, rx_buf_size);
	pthread_spin_unlock(&net->lock);
	return ret;
}

static __always_inline int
net_send_and_receive_zerocopy_lock(struct session_net *net,
				   void *tx_buf, size_t tx_buf_size,
				   void **rx_buf, size_t *rx_buf_size)
{
	int ret;

	pthread_spin_lock(&net->lock);
	ret = net_send_and_receive_zerocopy(net,
					    tx_buf, tx_buf_size,
					    rx_buf, rx_buf_size);
	pthread_spin_unlock(&net->lock);
	return ret;
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

/*
 * Given the host order @ip, fill in the @ip_str
 * @ip_str must be INET_ADDRSTRLEN bytes long.
 */
static inline int get_ip_str(unsigned int ip, char *ip_str)
{
	struct in_addr in_addr;

	in_addr.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in_addr, ip_str, INET_ADDRSTRLEN);
	return 0;
}

#endif /* _LEGOMEM_UAPI_NET_SESSION_H_ */
