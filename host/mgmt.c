/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * This file describes functions for handling management session requests.
 * It has various handlers running on normal hosts, but not monitor.
 * In terms of functionalaties, this file resembles legomem-board soc code.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/thpool.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "core.h"
#include "net/net.h"

/*
 * We only need to a single worker
 * thus inline handling
 */
#define NR_THPOOL_WORKERS	1
#define NR_THPOOL_BUFFER	1

static int TW_HEAD = 0;
static int TB_HEAD = 0;
static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

static __always_inline struct thpool_worker *
select_thpool_worker_rr(void)
{
	struct thpool_worker *tw;
	int idx;

	idx = TW_HEAD % NR_THPOOL_WORKERS;
	tw = thpool_worker_map + idx;
	TW_HEAD++;
	return tw;
}

static __always_inline struct thpool_buffer *
alloc_thpool_buffer(void)
{
	struct thpool_buffer *tb;
	int idx;

	idx = TB_HEAD % NR_THPOOL_BUFFER;
	tb = thpool_buffer_map + idx;
	TB_HEAD++;

	/*
	 * If this happens during runtime, it means:
	 * - ring buffer is not large enough
	 * - some previous handlers are too slow
	 */
	while (unlikely(ThpoolBufferUsed(tb))) {
		;
	}

	SetThpoolBufferUsed(tb);
	barrier();
	return tb;
}

static __always_inline void
free_thpool_buffer(struct thpool_buffer *tb)
{
	tb->flags = 0;
	barrier();
}

static void handle_close_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	unsigned int ip, port;
	char ip_str[INET_ADDRSTRLEN];
	struct board_info *bi;
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;
	struct session_net *ses_net;
	unsigned int dst_sesid;
	int ret;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	ipv4_hdr = to_ipv4_header(req);
	udp_hdr = to_udp_header(req);

	ip = ntohl(ipv4_hdr->src_ip);
	get_ip_str(ip, ip_str);
	port = ntohs(udp_hdr->src_port);

	bi = find_board(ip, port);
	if (!bi) {
		dprintf_ERROR("board not found %s:%d\n", ip_str, port);
		goto error;
	}

	/* Find if the session exist */
	dst_sesid = req->op.session_id;
	ses_net = find_net_session(ip, port, dst_sesid);
	if (!ses_net) {
		dprintf_ERROR("session not found %s:%d sesid %u\n",
			ip_str, port, dst_sesid);
		dump_net_sessions();
		goto error;
	}

	ret = generic_handle_close_session(NULL, bi, ses_net);
	if (ret) {
		dprintf_ERROR("fail to close session %s:%d sesid %u\n",
			ip_str, port, dst_sesid);
		dump_net_sessions();
		goto error;
	}

	dprintf_DEBUG("session closed, remote: %s remote sesid: %u local sesid: %u\n",
		bi->name, get_remote_session_id(ses_net), get_local_session_id(ses_net));

	/* Success */
	resp->op.session_id = 0;
	return;

error:
	resp->op.session_id = -EFAULT;
}

/*
 * Handle the case when a remote party wants to open a session
 * with us. It must tell us its local session id.
 */
static void handle_open_session(struct thpool_buffer *tb)
{
	struct legomem_open_close_session_req *req;
	struct legomem_open_close_session_resp *resp;
	unsigned int ip, port;
	struct board_info *bi;
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;
	struct session_net *ses_net;

	req = (struct legomem_open_close_session_req *)tb->rx;
	resp = (struct legomem_open_close_session_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	ipv4_hdr = to_ipv4_header(req);
	udp_hdr = to_udp_header(req);

	ip = ntohl(ipv4_hdr->src_ip);
	port = ntohs(udp_hdr->src_port);

	bi = find_board(ip, port);
	if (!bi) {
		char ip_str[INET_ADDRSTRLEN];
		get_ip_str(ip, ip_str);
		dprintf_ERROR("board not found %s:%d\n", ip_str, port);
		dump_boards();
		goto error;
	}

	ses_net = generic_handle_open_session(bi, req->op.session_id);
	if (!ses_net) {
		dprintf_ERROR("fail to open receiver side session. sender: %s\n",
			bi->name);
		goto error;
	}

	resp->op.session_id = get_local_session_id(ses_net);

	dprintf_DEBUG("session opened, remote: %s remote sesid: %u local sesid: %u\n",
		bi->name, get_remote_session_id(ses_net), get_local_session_id(ses_net));

	return;

error:
	resp->op.session_id = 0;
}

/*
 * Handle the case when _monitor_ notifies us
 * that there is a new node joining the cluster.
 * We will add it to our local list.
 */
static void handle_new_node(struct thpool_buffer *tb)
{
	struct legomem_membership_new_node_req *req;
	struct legomem_membership_new_node_resp *resp;
	struct endpoint_info *new_ei;
	struct board_info *bi;
	int ret, i;
	unsigned char mac[6];
	int ip;
	char *ip_str;

	/* Setup response */
	resp = (struct legomem_membership_new_node_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
	resp->ret = 0;

	/* Setup request */
	req = (struct legomem_membership_new_node_req *)tb->rx;
	new_ei = &req->op.ei;

	/* Sanity check */
	if (req->op.type != BOARD_INFO_FLAGS_HOST &&
	    req->op.type != BOARD_INFO_FLAGS_BOARD) {
		dprintf_ERROR("invalid type: %lu %s\n",
			req->op.type, board_info_type_str(req->op.type));
		return;
	}

	/*
	 * We may use a different local MAC address to reach the new host
	 * run our local ARP protocol to get the latest and update if necessary.
	 */
	ip = new_ei->ip;
	ip_str = (char *)new_ei->ip_str;
	ret = get_mac_of_remote_ip(ip, ip_str, global_net_dev, mac);
	if (ret) {
		dprintf_ERROR("fail to get mac of new node. ip %s\n", ip_str);
		return;
	}

	if (memcmp(mac, new_ei->mac, 6)) {
		printf("%s(): INFO mac updated ", __func__);
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", new_ei->mac[i]);
			else
				printf("%x -> ", new_ei->mac[i]);
		}
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", mac[i]);
			else
				printf("%x\n", mac[i]);
		}

		/* Override the original MAC */
		memcpy(new_ei->mac, mac, 6);
	}

	/* Finally add the board to the system */
	bi = add_board(req->op.name, req->op.mem_size_bytes,
		       new_ei, &default_local_ei, false);
	if (!bi)
		return;
	bi->flags = req->op.type;

	dprintf_INFO("new node added name: %s, ip:port: %s:%d type: %s\n",
		req->op.name, new_ei->ip_str, new_ei->udp_port,
		board_info_type_str(bi->flags));

	dump_boards();
	dump_net_sessions();
}

/*
 * Manually add a new remote node.
 * Since this is not broadcast from monitor, this information is local only.
 * This interface is mainly designed for testing purpose.
 */
int manually_add_new_node(unsigned int ip, unsigned int udp_port,
			  unsigned int node_type)
{
	struct thpool_buffer tb;
	struct legomem_membership_new_node_req *req;
	struct endpoint_info *ei;
	char ip_str[INET_ADDRSTRLEN];
	char new_name[BOARD_NAME_LEN];

	/* Cook the name */
	get_ip_str(ip, ip_str);
	if (node_type == BOARD_INFO_FLAGS_BOARD) {
		sprintf(new_name, "t_board_%s:%u", ip_str, udp_port);
	} else {
		dprintf_ERROR("Manual adding only supports _board_ for now. (%s)\n",
			board_info_type_str(node_type));
		return -EINVAL;
	}

	req = (struct legomem_membership_new_node_req *)tb.rx;

	/*
	 * We do not need to fill the mac addr
	 * let the handler figure it out
	 */
	ei = &req->op.ei;
	memcpy(ei->ip_str, ip_str, INET_ADDRSTRLEN);
	ei->ip = ip;
	ei->udp_port = udp_port;

	/* Fill in the fake request */
	req->op.type = node_type;
	req->op.mem_size_bytes = 0;
	memcpy(req->op.name, new_name, BOARD_NAME_LEN);

	handle_new_node(&tb);
	return 0;
}

int manually_add_new_node_str(const char *ip_port_str, unsigned int node_type)
{
	int ip, port;
	int ip1, ip2, ip3, ip4;

	sscanf(ip_port_str, "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;
	return manually_add_new_node(ip, port, node_type);
}

static void handle_pingpong(struct thpool_buffer *tb)
{
	struct legomem_pingpong_resp *resp;

	resp = (struct legomem_pingpong_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
}

static void
worker_handle_request_inline(struct thpool_worker *tw, struct thpool_buffer *tb)
{
	struct lego_header *lego_hdr;
	struct gbn_header *gbn_hdr;
	uint16_t opcode;
	struct routing_info *ri;

	lego_hdr = to_lego_header(tb->rx);
	opcode = lego_hdr->opcode;

	switch (opcode) {
	case OP_REQ_MEMBERSHIP_NEW_NODE:
		handle_new_node(tb);
		break;
	case OP_OPEN_SESSION:
		handle_open_session(tb);
		break;
	case OP_CLOSE_SESSION:
		handle_close_session(tb);
		break;
	case OP_REQ_PINGPONG:
		handle_pingpong(tb);
		break;
	default:
		dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
			opcode, legomem_opcode_str(opcode));
		goto free;
	};

	if (likely(!ThpoolBufferNoreply(tb))) {
		/*
		 * We the mgmt session accepting all traffics
		 * thus we do not really know who is the sender prior
		 * we can only infer that info from the incoming traffic
		 * To reply, we can only, and should, simply swap the routing info
		 */
		ri = (struct routing_info *)tb->rx;
		swap_routing_info(ri);

		/*
		 * Original must be X -> 0
		 * It will become 0 -> X
		 * (X is larger than 0)
		 */
		gbn_hdr = to_gbn_header(tb->rx);
		swap_gbn_session(gbn_hdr);

		net_send_with_route(mgmt_session, tb->tx, tb->tx_size, ri);
	}

free:
	free_thpool_buffer(tb);
}

static void *mgmt_handler_func(void *_unused)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	int ret;

	while (1) {
		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		ret = net_receive(mgmt_session, tb->rx, THPOOL_BUFFER_SIZE);
		if (ret <= 0)
			continue;
		tb->rx_size = ret;

		/* We only have one thread, thus inline handling */
		worker_handle_request_inline(tw, tb);
	}
	return NULL;
}

struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

/*
 * Open the local mgmt session
 * and its associated handler thread
 */
int init_local_management_session(bool create_mgmt_thread)
{
	struct endpoint_info dummy_ei = { 0 };
	pthread_t t;
	int ret;

	/* This is LOCAL dummy board */
	mgmt_dummy_board = add_board("special_local_mgmt", 0,
				     &dummy_ei, &dummy_ei,
				     true);
	if (!mgmt_dummy_board)
		return -ENOMEM;
	mgmt_dummy_board->flags |= BOARD_INFO_FLAGS_DUMMY;

	/* This is our LOCAL mgmt session */
	mgmt_session = get_board_mgmt_session(mgmt_dummy_board);

	if (!create_mgmt_thread)
		return 0;

	/*
	 * Normal host side needs this polling thread to handle
	 * mgmt session. But monitor will not need this thread
	 * because it will do-it-yourself. Besides, host and monitor
	 * have totoally different handlers.
	 */

	init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map,
			   default_thpool_buffer_alloc_cb);

	ret = pthread_create(&t, NULL, mgmt_handler_func, NULL);
	if (ret) {
		printf("%s(): fail to create the mgmt handler thread\n", __func__);
		return ret;
	}

	/* Save into session-local storage */
	mgmt_session->thread = t;
	return 0;
}
