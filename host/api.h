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
struct legomem_vregion;

enum legomem_write_flag {
	LEGOMEM_WRITE_SYNC,
	LEGOMEM_WRITE_ASYNC,
};

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

struct session_net *__find_or_alloc_vregion_session(struct legomem_context *ctx,
						    struct legomem_vregion *v);

struct session_net *find_or_alloc_vregion_session(struct legomem_context *ctx,
						  unsigned long __remote addr);

int legomem_read_with_session(struct legomem_context *ctx, struct session_net *ses,
			      void *send_buf, void *recv_buf,
			      unsigned long __remote addr, size_t total_size);
int __legomem_write_with_session(struct legomem_context *ctx, struct session_net *ses,
				 void *send_buf, unsigned long __remote addr, size_t total_size,
				 enum legomem_write_flag flag);

int legomem_read_with_session_msgbuf(struct legomem_context *ctx, struct session_net *ses,
				     struct msg_buf *send_mb, void *recv_buf,
				     unsigned long __remote addr, size_t total_size);

int __legomem_write_with_session_msgbuf(struct legomem_context *ctx, struct session_net *ses,
					struct msg_buf *send_mb, unsigned long __remote addr, size_t total_size,
					enum legomem_write_flag flag);

void __legomem_dist_barrier(void);
int legomem_dist_barrier(void);
void handle_dist_barrier(struct thpool_buffer *tb);

int legomem_pointer_chasing(struct legomem_context *ctx,
		            uint64_t __remote ptr,
			    uint64_t key, uint16_t structSize,
			    uint16_t valueSize, uint8_t keyOffset,
			    uint8_t valueOffset, uint8_t depth,
			    uint8_t nextOffset);

#endif /* _LEGOMEM_PUBLIC_APIS_ */
