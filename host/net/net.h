/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _HOST_NET_NET_H_
#define _HOST_NET_NET_H_

#include <errno.h>
#include <stdio.h>
#include <uapi/hashtable.h>
#include <uapi/net_header.h>
#include <uapi/net_session.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <infiniband/verbs.h>

extern int sysctl_link_mtu;

int init_net(struct endpoint_info *local_ei);
void dump_packet_headers(void *packet, char *str_buf);

struct session_net *net_open_session(struct endpoint_info *local_ei,
				     struct endpoint_info *remote_ei);
int net_close_session(struct session_net *ses);

struct session_raw_socket {
	int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct sockaddr_ll saddr;
};

struct session_udp_socket {
	int sockfd;
	struct sockaddr_in remote_addr;
};

struct session_raw_verbs {
	struct ibv_pd *pd;
	struct ibv_qp *qp;
	struct ibv_cq *send_cq;

	struct ibv_cq *recv_cq;
	struct ibv_mr *recv_mr;
	void *recv_buf;

	/*
	 * Registered via net_reg_send_buf
	 * Each session would only have one registerd send buffer.
	 * A session is used by one thread, if it is extended to
	 * use coroutine, it would be per-coroutine.
	 *
	 * This is per session local.
	 * Others are shared, check open_session code.
	 * We reused the cached_session_raw_verbs
	 */
	struct ibv_mr *send_mr;
	void *send_buf;
	size_t send_buf_size;

	struct ibv_flow *eth_flow;

	pthread_spinlock_t *lock;
};

#endif /* _HOST_NET_NET_H_ */
