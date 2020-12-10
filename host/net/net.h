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
#include <uapi/compiler.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <infiniband/verbs.h>
#include <stdarg.h>
#include <stdatomic.h>

extern int sysctl_link_mtu;

int init_net(struct endpoint_info *local_ei);
void dump_packet_headers(void *packet, char *str_buf);
void __dump_packet_headers(void *packet, char *str_buf);

struct session_net *net_open_session(struct endpoint_info *local_ei,
				     struct endpoint_info *remote_ei);
int net_close_session(struct session_net *ses);

struct session_raw_socket {
	int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct sockaddr_ll saddr;
} ____cacheline_aligned;

struct session_udp_socket {
	int sockfd;
	struct sockaddr_in remote_addr;
} ____cacheline_aligned;


/*
 * Raw Verbs
 */
#define BUFFER_SIZE			(2048)	/* maximum size of each send buffer */
#define NR_BUFFER_DEPTH			(256)
#define MAX_RECV_BUFFER_SIZE		(BUFFER_SIZE * NR_BUFFER_DEPTH)

#define NR_MAX_OUTSTANDING_SEND_WR	(1)
#define NR_BATCH_POST_RECV		(32)
#define NR_MAX_RECV_BATCH		(32)

struct session_raw_verbs {
	struct ibv_pd *pd;
	struct ibv_qp *qp;

	struct ibv_cq *send_cq;
	unsigned long nr_post_send;

	struct ibv_cq *recv_cq;
	struct ibv_mr *recv_mr;
	void *recv_buf;

	struct ibv_mr *send_mr;
	void *send_buf;
	size_t send_buf_size;

	struct ibv_flow 	*eth_flow;
	unsigned int		rx_udp_port;

	struct ibv_wc		send_wc[NR_MAX_OUTSTANDING_SEND_WR];

	struct ibv_sge		recv_sge[NR_BUFFER_DEPTH];
	struct ibv_recv_wr	recv_wr[NR_BUFFER_DEPTH];
	struct ibv_wc		recv_wc[NR_MAX_RECV_BATCH];

	int recv_head;
	int nr_delayed_recvs;
} ____cacheline_aligned;

void dump_gbn_session(struct session_net *net, bool dump_rx_ring);
struct ibv_mr *raw_verbs_reg_mr(void *buf, size_t buf_size);

#endif /* _HOST_NET_NET_H_ */
