/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * This file has shared handlers used by all host/monitor/board_emulator.
 * It must be generic, things like handle_pingpong, session etc.
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

void handle_close_session(struct thpool_buffer *tb)
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
	ses_net = find_net_session(dst_sesid);
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
void handle_open_session(struct thpool_buffer *tb)
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
void handle_new_node(struct thpool_buffer *tb)
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

#if 0
	/*
	 * oOh jesus this printf causes weird bug.
	 * So this handle_new_node() is called within worker_handle_request_inline(),
	 * with passed @tb parameter. When this function is enabled, the @tb within
	 * the caller stack, changed to 0. It seems this printf cause some overflow?
	 * Or maybe the gcc has some internal bug? Weirdly, if I enabled gdb + watchpoint,
	 * it succeed.. Either way, I don't get this.
	 */
	dprintf_INFO("new node added name: %s, ip:port: %s:%d type: %s\n",
		req->op.name, new_ei->ip_str, new_ei->udp_port,
		board_info_type_str(bi->flags));
#endif

	dump_boards();
	dump_net_sessions();
}

/*
 * Handle a pingpong request.
 * The reply size is specificed by sender, but we enforce a cap.
 */
void handle_pingpong(struct thpool_buffer *tb)
{
	struct legomem_pingpong_req *req;
	int reply_size;
#define HANDLE_PINGPONG_MAX_REPLY_SIZE (4096)

	req = (struct legomem_pingpong_req *)tb->rx;

	reply_size = req->reply_size + sizeof(struct legomem_pingpong_resp);
	if (reply_size > HANDLE_PINGPONG_MAX_REPLY_SIZE)
		reply_size = HANDLE_PINGPONG_MAX_REPLY_SIZE;
	set_tb_tx_size(tb, reply_size);
}

/*
 * Server side session handler.
 * just for debugging and measurement, i guess
 *
 * The reason to have this handler: when a sender wants to open a new session
 * with a receiver, the receiver side user code does not know when this event
 * would happen and what's the session id without another layer of msg exchange.
 *
 * Thus, we took a different approach at receiver side: whenever a recever
 * gets a open_session request, it will proactively launch a new thread
 * running this handler (in generic_handle_close_session()).
 */
static int tmp_cpu = 1;
void *user_session_handler(void *_ses)
{
	struct thpool_buffer tb = { 0 };
	struct lego_header *lego_header;
	struct session_net *ses;
	int ret, opcode;
	int cpu, node;

	ses = (struct session_net *)_ses;

	tb.tx = malloc(THPOOL_BUFFER_SIZE);
	if (!tb.tx) {
		dprintf_ERROR("OOM %d\n", 0);
		return NULL;
	}

	ret = net_reg_send_buf(ses, tb.tx, THPOOL_BUFFER_SIZE);
	if (ret) {
		dprintf_ERROR("Fail to register TX buffer %d\n", ret);
		return NULL;
	}

	pin_cpu(tmp_cpu++);
	getcpu(&cpu, &node);
	dprintf_DEBUG("CPU=%d Node=%d, for session local_id = %u remote_id = %u tb.tx=%#lx\n",
		cpu, node, get_local_session_id(ses), get_remote_session_id(ses), (unsigned long)tb.tx);

	while (1) {
		if (unlikely(ses_thread_should_stop(ses)))
			break;

		ret = net_receive_zerocopy(ses, &tb.rx, &tb.rx_size);
		if (ret <= 0)
			continue;

		lego_header = to_lego_header(tb.rx);
		opcode = lego_header->opcode;
		switch (opcode) {
		case OP_REQ_PINGPONG:
			handle_pingpong(&tb);
			break;
		default:
			dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
				      opcode, legomem_opcode_str(opcode));
			set_tb_tx_size(&tb, sizeof(struct legomem_common_headers));
			break;
		}

		if (likely(!ThpoolBufferNoreply(&tb)))
			net_send(ses, tb.tx, tb.tx_size);
		tb.flags = 0;
	}

	free(tb.tx);
	return NULL;
}
