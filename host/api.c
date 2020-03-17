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

struct session_net *monitor_session;

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
		 * Normal context creation
		 * Contact monitor
		 */
		struct legomem_create_context_req req;
		struct legomem_create_context_resp resp;
		struct lego_header *lego_header;

		lego_header = to_lego_header(&req);
		lego_header->opcode = OP_CREATE_PROC;
		memset(req.op.proc_name, 'a', PROC_NAME_LEN);

		net_send(monitor_session, &req, sizeof(req));
		net_receive(monitor_session, &resp, sizeof(resp));

		if (unlikely(resp.op.ret))
			goto err;

		p->pid = resp.op.pid;
	}

	return p;

err:
	remove_legomem_context(p);
	return NULL;
}

struct legomem_context *legomem_open_context(void)
{
	return __legomem_open_context(false);
}

struct legomem_context *legomem_open_context_mgmt(void)
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
		struct legomem_close_context_req req;
		struct legomem_close_context_resp resp;
		struct lego_header *lego_header;

		lego_header = to_lego_header(&req);
		lego_header->opcode = OP_FREE_PROC;
		lego_header->pid = ctx->pid;

		net_send_and_receive(monitor_session, &req, sizeof(req),
				     &resp, sizeof(resp));

		if (resp.ret) {
			printf("%s(): monitor fail to close a session. ret %d\n",
				__func__, resp.ret);
		}
	}

	free(ctx);
	return 0;
}

/*
 * Open a new network session with a board.
 */
static struct session_net *
__legomem_open_session(struct legomem_context *ctx, struct board_info *bi,
		       pid_t tid, bool is_mgmt)
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
	ses->tid = tid;

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
	pid_t tid = gettid();
	return __legomem_open_session(ctx, bi, tid, false);
}

/*
 * Internal API
 * Called once during startup to open a management network session
 */
struct session_net *
legomem_open_session_mgmt(struct legomem_context *ctx, struct board_info *bi)
{
	return __legomem_open_session(ctx, bi, 0, true);
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

/*
 * This function tries to find an already established vregion that could
 * possibily allocate @size mem for us. Return the vRegion if found one,
 * NULL otherwise and caller needs to contact monitor.
 */
struct legomem_vregion *
find_vregion_candidate(struct legomem_context *p, size_t size)
{
	struct legomem_vregion *v;
	int i;

	if (p->cached_vregion)
		i = p->cached_vregion - p->vregion;
	else
		i = 0;

	for ( ; i < NR_VREGIONS; i++) {
		v = p->vregion + i;
		if (v->avail_space >= size) {
			p->cached_vregion = v;
			return v;
		}
	}
	return NULL;
}

static int
ask_monitor_for_new_vregion(struct legomem_context *ctx, size_t size,
			    unsigned int *board_ip, unsigned int *vregion_idx)
{
	struct legomem_alloc_free_req req;
	struct legomem_alloc_free_resp resp;
	struct lego_header *lego_header;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_ALLOC;
	lego_header->pid = ctx->pid;

	req.op.len = size;
	req.op.vm_flags = 0;

	ret = net_send_and_receive(monitor_session, &req, sizeof(req),
				   &resp, sizeof(resp));
	if (ret <= 0)
		return -EIO;

	if (resp.op.ret)
		return resp.op.ret;

	/* success */
	*board_ip = resp.op.board_ip;
	*vregion_idx = resp.op.vregion_idx;
	return 0;
}

unsigned long __remote
legomem_alloc(struct legomem_context *ctx, size_t size)
{
	struct legomem_alloc_free_req req;
	struct legomem_alloc_free_resp resp;
	struct lego_header *lego_header;
	struct legomem_vregion *v;
	struct session_net *ses;
	unsigned int board_ip, vregion_idx;
	int ret;
	pid_t tid;
	unsigned long __remote addr;

	/*
	 * First try to find an existing
	 * vregion that could possibly fulfill this request:
	 */
	v = find_vregion_candidate(ctx, size);
	if (v) {
		ses = v->ses_net;
		BUG_ON(!ses);
		goto found;
	}

	/*
	 * Otherwise we ask monitor to alloc a new vRegion
	 * and it will tell use the new board and vRegion index
	 */
	ret = ask_monitor_for_new_vregion(ctx, size, &board_ip, &vregion_idx);
	if (ret)
		return 0;

	/*
	 * Finally check if this context and this thread
	 * already has an established session with the new board
	 */
	tid = gettid();
	ses = context_find_session_by_ip(ctx, tid, board_ip);
	if (!ses) {
		/*
		 * If there was no session before,
		 * we need to open a new one:
		 */
		struct board_info *bi;

		bi = find_board_by_ip(board_ip);
		if (unlikely(bi)) {
			printf("WARN board_ip %x not found. "
			       "Please add it during start.\n", board_ip);
			return 0;
		}

		ses = legomem_open_session(ctx, bi);
		if (unlikely(!ses))
			return 0;
	}

	/*
	 * At this point we have the new vRegion
	 * and a new session (or an old one), we simply
	 * link them together:
	 */
	v = index_to_legomem_vregion(ctx, vregion_idx);
	v->ses_net = ses;

found:
	/*
	 * All good, once we are here, it means
	 * we have a valid vRegion and net session.
	 */
	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_ALLOC;

	req.op.len = size;

	ret = net_send_and_receive(ses, &req, sizeof(req), &resp, sizeof(resp));
	if (ret <= 0) {
		printf("%s(): RPC failed\n", __func__);
		return ret;
	}

	/* Check response from the board */
	if (resp.op.ret != 0) {
		printf("%s(): legomem_alloc failure %d.\n",
			__func__, resp.op.ret);
		return 0;
	}

	addr = resp.op.addr;
	return addr;
}

int legomem_free(struct legomem_context *ctx,
		 unsigned long __remote addr, size_t size)
{
	struct legomem_vregion *v;
	struct session_net *ses;
	struct legomem_alloc_free_req req;
	struct legomem_alloc_free_resp resp;
	struct lego_header *lego_header;
	int ret;

	v = va_to_legomem_vregion(ctx, addr);
	ses = v->ses_net;
	if (unlikely(!ses)) {
		printf("%s(): addr %#lx was not allocated.\n",
			__func__, addr);
		return -EINVAL;
	}

	/* Prepare headers */
	lego_header = to_lego_header((void *)&req);
	lego_header->opcode = OP_REQ_FREE;

	req.op.addr = addr;
	req.op.len = size;

	ret = net_send(ses, &req, sizeof(req));
	if (ret <= 0) {
		printf("%s(): fail to send req\n", __func__);
		return -EIO;
	}

	ret = net_receive(ses, &resp, sizeof(resp));
	if (ret <= 0) {
		printf("%s() fail to recv msg\n", __func__);
		return -EIO;
	}
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
	struct lego_header *lego_header;
	int ret;

	v = va_to_legomem_vregion(ctx, addr);
	ses = v->ses_net;
	if (unlikely(!ses)) {
		printf("%s(): remote addr %#lx has no established session.\n",
			__func__, addr);
		return -EINVAL;
	}

	lego_header = to_lego_header((void *)&req);
	lego_header->opcode = OP_REQ_READ;

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
	struct lego_header *lego_header;
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

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_WRITE;
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
