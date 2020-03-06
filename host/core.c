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
struct legomem_context *legomem_open_context(void)
{
	struct legomem_context *p;
	int ret;

	p = malloc(sizeof(*p));
	if (!p)
		return NULL;

	/* Add to per-node context list */
	ret = add_legomem_context(p);
	if (ret) {
		free(p);
		return NULL;
	}

	/*
	 * TODO
	 * 1. contact monitor for PID
	 * 2. open sessions, if necessary
	 */

	return p;
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

	/* TODO Contact Monitor */

	free(ctx);
	return 0;
}

/*
 * Open a new network session with a board.
 */
struct session_net *
legomem_open_session(struct legomem_context *ctx, struct board_info *bi)
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

	/*
	 * TODO Contact monitor/board to alloc the connection
	 * Mainly to get the connection ID!
	 */

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
 * Close a open network session.
 */
int legomem_close_session(struct legomem_context *ctx, struct session_net *ses)
{
	struct board_info *bi;

	/*
	 * TODO Contact monitor/board to free the connection
	 */

	/*
	 * Clear bookkeeping we've done
	 * when the session was open:
	 */
	bi = ses->board_info;
	remove_net_session(ses);
	board_remove_session(bi, ses);
	context_remove_session(ctx, ses);

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
	return 0;
}

int legomem_write(struct legomem_context *ctx, void *buf,
		  unsigned long __remote addr, size_t size)
{
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

	test_app(local_ei, remote_ei);

	return 0;
}
