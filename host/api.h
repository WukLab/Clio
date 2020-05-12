/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _LEGOMEM_PUBLIC_APIS_
#define _LEGOMEM_PUBLIC_APIS_

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/opcode.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/thpool.h>
#include "net/net.h"
#include <limits.h>
#include <time.h>
#include <pthread.h>

struct legomem_context;

/*
 * LegoMem Public APIs
 */
struct legomem_context *legomem_open_context(void);
int legomem_close_context(struct legomem_context *ctx);
struct session_net *legomem_open_session(struct legomem_context *ctx, struct board_info *bi);
struct session_net *generic_handle_open_session(struct board_info *bi, unsigned int dst_sesid);
int generic_handle_close_session(struct legomem_context *ctx,
				 struct board_info *bi,
				 struct session_net *ses);
struct session_net *
legomem_open_session_remote_mgmt(struct board_info *bi);
struct session_net *
legomem_open_session_local_mgmt(struct board_info *bi);
int legomem_close_session(struct legomem_context *ctx, struct session_net *ses);
unsigned long __remote
legomem_alloc(struct legomem_context *ctx, size_t size, unsigned long vm_flags);
int legomem_free(struct legomem_context *ctx,
		 unsigned long __remote addr, size_t size);
int legomem_read(struct legomem_context *ctx, void *send_buf, void *recv_buf,
		 unsigned long __remote addr, size_t size);
int legomem_write_sync(struct legomem_context *ctx, void *send_buf,
		       unsigned long __remote addr, size_t size);
int legomem_write_async(struct legomem_context *ctx, void *send_buf,
			unsigned long __remote addr, size_t size);
int legomem_migration(struct legomem_context *ctx, struct board_info *dst_bi,
		      unsigned long __remote addr, unsigned long size);

struct session_net *
get_vregion_session(struct legomem_context *ctx, unsigned long __remote addr);

#endif /* _LEGOMEM_PUBLIC_APIS_ */
