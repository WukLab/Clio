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
#include "api_kvs.h"

/*
 * Request/Response Packet Layout:
 * [ETH/IP/UDP/GBN/Lego/op_kvs_req/value../]
 *
 * (If we use RPC interface there won't be any GBN headers)
 *
 * We are using lego->opcode field for KVS opcodes as well.
 * op_kvs_req describes kv pairs. Please refer to include/uapi/opcode.h.
 */


/*
 * @value: value buffer to write.
 */
static int
__legomem_kvs_write(struct legomem_context *ctx, struct session_net *ses, uint16_t opcode,
		    uint16_t key_size, char *key, uint16_t value_size, void *value)
{
	struct legomem_kvs_req *req;
	struct lego_header *tx_lego;
	struct op_kvs_req *op;
	struct legomem_kvs_resp *resp;
	size_t recv_size;

	req = net_get_send_buf(ses);

	/* Cook lego headers */
	tx_lego = to_lego_header(req);
	tx_lego->opcode = opcode;
	tx_lego->pid = ctx->pid;

	/*
	 * SuperNIC Specific
	 * Copy the 8B to this location so
	 * that the whole key is contained within the
	 * first 64B flit.
	 */
	*(unsigned long *)(&(tx_lego->cont)) = (unsigned long)key;

	/* Cook KVS-specific headers */
	op = &req->op;
	op->key_size = key_size;
	op->key = key;
	op->value_size = value_size;

	/*
	 * Copy user provided value buffers.
	 * If user can provide a larger buffer (i.e., reserve
	 * space for common_headers), we could avoid this.
	 */
	//memcpy(op->value, value, value_size);

	net_send(ses, req, sizeof(*req) + value_size);
	net_receive_zerocopy(ses, (void **)&resp, &recv_size);
#if 0
	if (opcode == OP_REQ_KVS_CREATE) {
		net_send(ses, req, sizeof(*req) + value_size);
		net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	} else {
		net_send(ses, req, sizeof(*req) + value_size);
		net_send(ses, req, sizeof(*req) + value_size);
		net_receive_zerocopy(ses, (void **)&resp, &recv_size);
		net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	}
#endif

	return 0;
}

int legomem_kvs_create(struct legomem_context *ctx, struct session_net *ses, uint16_t key_size, 
		      char *key, uint16_t value_size, void *value)
{
	return __legomem_kvs_write(ctx, ses, OP_REQ_KVS_CREATE, key_size, key, value_size, value);
}

int legomem_kvs_update(struct legomem_context *ctx, struct session_net *ses, uint16_t key_size,
		       char *key, uint16_t value_size, void *value)
{
	return __legomem_kvs_write(ctx, ses, OP_REQ_KVS_UPDATE, key_size, key, value_size, value);
}

int legomem_kvs_read(struct legomem_context *ctx, struct session_net *ses, uint16_t key_size,
		     char *key, uint16_t value_size, void *value)
{
	struct legomem_kvs_req *req;
	struct lego_header *tx_lego;
	struct lego_header *rx_lego __maybe_unused;
	struct op_kvs_req *op;
	struct legomem_kvs_resp *resp;
	int ret;
	size_t recv_size;

	req = net_get_send_buf(ses);

	tx_lego = to_lego_header(req);
	tx_lego->opcode = OP_REQ_KVS_READ;
	tx_lego->pid = ctx->pid;

	op = &req->op;
	op->key_size = key_size;
	op->key = key;
	op->value_size = value_size;

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
#if 0
	rx_lego = to_lego_header(resp);
	if (unlikely(rx_lego->req_status)) {
		dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
		return -1;
	}
#endif

	/*
	 * Zerocopy is using low-level network ring-buffers,
	 * which will be recycled soon. We should copy to user buffer.
	 */
	//memcpy(value, resp->op.value, value_size);

	return 0;
}

int legomem_kvs_delete(struct legomem_context *ctx, struct session_net *ses, 
		uint16_t key_size, char *key)
{
	struct legomem_kvs_req *req;
	struct lego_header *tx_lego;
	struct op_kvs_req *op;
	struct legomem_kvs_resp *resp;
	int ret;
	size_t recv_size;

	req = net_get_send_buf(ses);

	tx_lego = to_lego_header(req);
	tx_lego->opcode = OP_REQ_KVS_DELETE;
	tx_lego->pid = ctx->pid;

	op = &req->op;
	op->key_size = key_size;
	op->key = key;

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
