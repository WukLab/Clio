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
 * Request/Response Packet Layout:
 * [ETH/IP/UDP/GBN/Lego/op_mvobj/.../]
 *
 * TODO
 * Figrue out struct layout, opcode position.
 * Build APIs etc.
 */

/*
 * @value: value buffer to write.
 */
int legomem_mv_write(struct legomem_context *ctx,
		     uint32_t objid, uint16_t value_size, void *value)
{
	struct legomem_verobj_read_write_req *req;
	struct legomem_verobj_read_write_resp *resp;
	struct op_verobj_read_write *op;
	struct lego_header *tx_lego;
	struct session_net *ses;
	struct board_info *bi;
	int ret;
	size_t recv_size;

	// TODO
	// Find remote board
	// find a session associated with this board
	// find the pre-created memory regiion buffer
	bi = NULL;
	ses = NULL;
	req = NULL;

	/* Cook lego headers */
	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_VEROBJ_WRITE;

	/* Cook multi version data store parameter */
	op = &req->op;
	op->obj_id = objid;
	op->version = 0;

	/*
	 * Copy user provided value buffers.
	 * If user can provide a larger buffer (i.e., reserve
	 * space for common_headers), we could avoid this.
	 */
	memcpy(op->data, value, value_size);

	ret = net_send(ses, req, sizeof(*req) + value_size);
	if (ret < 0) {
		dprintf_ERROR("Net send error %d\n", ret);
		return ret;
	}
	ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	if (ret < 0) {
		dprintf_ERROR("Net recv error %d\n", ret);
		return ret;
	}
	return 0;
}

static int
__legomem_mv_read(struct legomem_context *ctx,
		  uint32_t objid, uint16_t version, uint16_t value_size, void *value)
{
	struct legomem_verobj_read_write_req *req;
	struct legomem_verobj_read_write_resp *resp;
	struct op_verobj_read_write *op;
	struct lego_header *tx_lego;
	struct lego_header *rx_lego;
	struct session_net *ses;
	struct board_info *bi;
	int ret;
	size_t recv_size;

	// TODO
	bi = NULL;
	ses = NULL;
	req = NULL;

	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_VEROBJ_READ;

	op = &req->op;
	op->obj_id = objid;
	op->version = version;

	ret = net_send(ses, req, sizeof(*req));
	if (ret < 0) {
		dprintf_ERROR("Net send error %d\n", ret);
		return ret;
	}

	ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	if (ret < 0) {
		dprintf_ERROR("Net recv error %d\n", ret);
		return ret;
	}
	rx_lego = to_lego_header(resp);
	if (unlikely(rx_lego->req_status)) {
		dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
		return -1;
	}

	/*
	 * Zerocopy is using low-level network ring-buffers,
	 * which will be recycled soon. We should copy to user buffer.
	 */
	memcpy(value, resp->op.data, value_size);

	return 0;
}

int legomem_mv_read_old(struct legomem_context *ctx, uint32_t objid,
		        uint16_t version, uint16_t value_size, void *value)
{
	return __legomem_mv_read(ctx, objid, version, value_size, value);
}

int legomem_mv_read_latest(struct legomem_context *ctx, uint32_t objid,
		          uint16_t value_size, void *value)
{
	return __legomem_mv_read(ctx, objid, 0, value_size, value);
}

int legomem_mv_create(struct legomem_context *ctx,
		      uint16_t value_size, uint64_t vm_flags, uint32_t* objid)
{
	struct legomem_verobj_create_delete_req *req;
	struct legomem_verobj_create_delete_resp *resp;
	struct op_verobj_create_delete *op;
	struct lego_header *tx_lego;
	struct session_net *ses;
	struct board_info *bi;
	int ret;
	size_t recv_size;

	// TODO
	// Find remote board
	// find a session associated with this board
	// find the pre-created memory regiion buffer
	bi = NULL;
	ses = NULL;
	req = NULL;

	/* Cook lego headers */
	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_VEROBJ_CREATE;

	/* TODO: vregion_idx Cook multi version data store parameter */
	op = &req->op;
	op->obj_size_id = value_size;
	op->vm_flags = vm_flags;

	ret = net_send(ses, req, sizeof(*req));
	if (ret < 0) {
		dprintf_ERROR("Net send error %d\n", ret);
		return ret;
	}
	ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	if (ret < 0) {
		dprintf_ERROR("Net recv error %d\n", ret);
		return ret;
	}

	*objid = (uint32_t)resp->op.obj_id;
	return 0;
}


int legomem_mv_delete(struct legomem_context *ctx, uint32_t objid)
{
	struct legomem_verobj_create_delete_req *req;
	struct legomem_verobj_create_delete_resp *resp;
	struct op_verobj_create_delete *op;
	struct lego_header *tx_lego;
	struct session_net *ses;
	struct board_info *bi;
	int ret;
	size_t recv_size;

	// TODO
	// Find remote board
	// find a session associated with this board
	// find the pre-created memory regiion buffer
	bi = NULL;
	ses = NULL;
	req = NULL;

	/* Cook lego headers */
	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_VEROBJ_DELETE;

	/* Cook multi version data store parameter */
	op = &req->op;
	op->obj_size_id = objid;

	ret = net_send(ses, req, sizeof(*req));
	if (ret < 0) {
		dprintf_ERROR("Net send error %d\n", ret);
		return ret;
	}
	ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	if (ret < 0) {
		dprintf_ERROR("Net recv error %d\n", ret);
		return ret;
	}
	return 0;
}
