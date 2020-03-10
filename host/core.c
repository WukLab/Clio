/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "net/net.h"

/*
 * Allocate a new process-local legomem context.
 * Monitor will be contacted. On success, the context is returned.
 */
static struct legomem_context *
__legomem_open_context(bool is_mgmt)
{
	struct legomem_context *p;
	int ret;

	p = malloc(sizeof(*p));
	if (!p)
		return NULL;

	init_legomem_context(p);

	/* Add to per-node context list */
	ret = add_legomem_context(p);
	if (ret) {
		free(p);
		return NULL;
	}

	if (is_mgmt) {
		p->flags |= LEGOMEM_CONTEXT_FLAGS_MGMT;
	} else {
		/*
		 * TODO
		 * 1. contact monitor for PID
		 * 2. open sessions, if necessary
		 */
	}
	return p;
}

struct legomem_context *legomem_open_context(void)
{
	return __legomem_open_context(false);
}

static struct legomem_context *legomem_open_context_mgmt(void)
{
	return __legomem_open_context(true);
}

/*
 * Close a given legomem context. Monitor will be contacted.
 * All resource associated with this context will be freed.
 */
int legomem_close_context(struct legomem_context *ctx)
{
	int ret;

	if (!ctx)
		return -EINVAL;

	/* Remove from per-node context list */
	ret = remove_legomem_context(ctx);
	if (ret)
		return ret;

	if (!(ctx->flags & LEGOMEM_CONTEXT_FLAGS_MGMT)) {
		/*
		 * TODO
		 * Contact Monitor to ask it to free
		 * all resources. Maybe boards too.
		 */
	}

	free(ctx);
	return 0;
}

/*
 * Open a new network session with a board.
 */
static struct session_net *
__legomem_open_session(struct legomem_context *ctx, struct board_info *bi,
		       bool is_mgmt)
{
	struct session_net *ses;

	if (!ctx || !bi)
		return NULL;

	/* 
	 * Ask network layer to prepare the data structures for this
	 * new session. Session ID is not allocated yet.
	 */
	ses = net_open_session(&bi->local_ei, &bi->remote_ei);
	if (!ses)
		return NULL;

	ses->board_ip = bi->board_ip;
	ses->board_info = bi;

	if (is_mgmt) {
		ses->session_id = LEGOMEM_MGMT_SESSION_ID;
	} else {
		/*
		 * TODO
		 * Contact monitor/board to alloc the conn ID!
		 */
		static int __tmp_id = 1;
		ses->session_id = __tmp_id++;
	}

	/*
	 * Bookkeeping, add to:
	 * - per-node session list
	 * - per-board session list
	 * - per-context session list
	 */
	add_net_session(ses);
	board_add_session(bi, ses);
	context_add_session(ctx, ses);
	return ses;
}

/*
 * Public API
 * Open a normal network connection.
 */
struct session_net *
legomem_open_session(struct legomem_context *ctx, struct board_info *bi)
{
	return __legomem_open_session(ctx, bi, false);
}

/*
 * Internal API
 * Called once during startup to open a management network session
 */
static struct session_net *
legomem_open_session_mgmt(struct legomem_context *ctx, struct board_info *bi)
{
	return __legomem_open_session(ctx, bi, true);
}

/*
 * Close a open network session.
 */
int legomem_close_session(struct legomem_context *ctx, struct session_net *ses)
{
	struct board_info *bi;

	if (!test_management_session(ses)) {
		/*
		 * TODO
		 * Contact monitor/board to free the connection
		 */
	}

	/*
	 * Clear bookkeeping we've done
	 * when the session was open:
	 */
	bi = ses->board_info;
	remove_net_session(ses);
	board_remove_session(bi, ses);
	context_remove_session(ctx, ses);

	/* Finally ask network layer to free resources */
	net_close_session(ses);

	return 0;
}

unsigned long __remote
legomem_alloc(struct legomem_context *ctx, size_t size)
{
	/*
	 * TODO
	 *
	 * Contact Monitor, which will return the board ID
	 * We need to use check if we already had a conneciton with that board.
	 * Use context_find_session()
	 */
	return 0;
}

int legomem_free(struct legomem_context *ctx,
		 unsigned long __remote addr, size_t size)
{
	return 0;
}

/*
 * Ctx+addr -> vregion -> board + connection
 */
int legomem_read(struct legomem_context *ctx, void *buf,
		 unsigned long __remote addr, size_t size)
{
	struct legomem_vregion *v;
	struct session_net *ses;
	struct legomem_read_write_req req;
	struct legomem_read_write_resp *resp;
	int ret;

	v = va_to_legomem_vregion(ctx, addr);
	ses = v->ses_net;
	if (unlikely(!ses)) {
		printf("%s(): remote addr %#lx has no established session.\n",
			__func__, addr);
		return -EINVAL;
	}

	req.op.va = addr;
	req.op.size = size;

	ret = net_send(ses, &req, sizeof(req));
	if (ret <= 0) {
		printf("%s(): fail to send req\n", __func__);
		return -EIO;
	}

	/*
	 * TODO
	 * temporary solution: if user just provide arbitrary buffer
	 * we cannot use it to recv msg because of extra headers.
	 * And we need to pay extra malloc/memcpy/free to it.
	 *
	 * An ideal case is to make user aware of this. Or provide a 
	 * msg/buffer alloc API for users.
	 */
	resp = malloc(size + sizeof(*resp));
	ret = net_receive(ses, resp, size + sizeof(*resp));
	if (ret <= 0) {
		printf("%s() fail to recv msg\n", __func__);
		return -EIO;
	}

	memcpy(buf, resp->ret.data, size);
	free(resp);
	return 0;
}

int legomem_write(struct legomem_context *ctx, void *buf,
		  unsigned long __remote addr, size_t size)
{
	struct legomem_vregion *v;
	struct session_net *ses;
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp resp;
	int ret;

	v = va_to_legomem_vregion(ctx, addr);
	ses = v->ses_net;
	if (unlikely(!ses)) {
		printf("%s(): remote addr %#lx has no established session.\n",
			__func__, addr);
		return -EINVAL;
	}

	/*
	 * TODO
	 * same as legomem_read.
	 *
	 * The downside of a "CPU transport" showcase here.
	 * I.e., we need to explicitly manage the buffers and reserve
	 * space for headers.
	 *
	 * If this is a hardware transport, everything can be attached
	 * by hardware on the fly.
	 */
	req = malloc(size + sizeof(*req));
	req->op.va = addr;
	req->op.size = size;
	memcpy(req->op.data, buf, size);

	ret = net_send(ses, req, size + sizeof(*req));
	if (ret <= 0) {
		printf("%s(): fail to send\n", __func__);
		return -EIO;
	}

	ret = net_receive(ses, &resp, sizeof(resp));
	if (ret <= 0) {
		printf("%s(): fail to receive\n", __func__);
		return -EIO;
	}

	if (resp.ret.ret != 0) {
		printf("%s(): fail to perform legomem write\n", __func__);
	}
	free(req);

	return 0;
}

/*
 * TODO
 *
 * We will remove thise manual coding later.
 * We will build a simple membership management during runtime.
 * A UDP port will receive and broadcast this info.
 */
struct endpoint_info ei_wuklab02 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
	.ip		= 0xc0a80102, /* 192.168.1.2 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab05 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xe4, 0x81, 0x51 },
	.ip		= 0xc0a80105, /* 192.168.1.5 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab06 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb3, 0x54, 0x11 },
	.ip		= 0xc0a80106, /* 192.168.1.6 */
	.udp_port	= 8888,
};
struct endpoint_info board_0 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
	.ip		= 0xc0a801c8, /* 192.168.1.200 */
	.udp_port	= 1234,
};
struct endpoint_info board_1 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0x00, 0x00 },
	.ip		= 0xc0a80180,
	.udp_port	= 1234,
};

void test_app(struct endpoint_info *, struct endpoint_info *);

struct legomem_context *mgmt_context;
struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

int init_management_session(void)
{
	struct endpoint_info dummy_ei;

	mgmt_dummy_board = add_board("local_mgmt", 0, &dummy_ei, &dummy_ei);
	if (!mgmt_dummy_board)
		return -ENOMEM;

	mgmt_context = legomem_open_context_mgmt();
	if (!mgmt_context)
		return -ENOMEM;

	mgmt_session = legomem_open_session_mgmt(mgmt_context, mgmt_dummy_board);
	if (!mgmt_session)
		return -ENOMEM;
	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	struct endpoint_info *local_ei = &ei_wuklab02;
	struct endpoint_info *remote_ei = &ei_wuklab05;

	ret = init_net(local_ei);
	if (ret) {
		printf("Fail to init network layer.\n");
		exit(-1);
	}

	/* Mainly init spinlocks */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	/* Open the mgmt session, aka session_0 */
	init_management_session();

	test_app(local_ei, remote_ei);

	return 0;
}
