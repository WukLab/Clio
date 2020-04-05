/*
 * Copyright (c) 2020，Wuklab, UCSD.
 *
 * This file defines the request opcodes used in LegoMem requests.
 * This file is used by both host, FPGA, and SoC.
 */

#ifndef _LEGOFPGA_OPCODE_H_
#define _LEGOFPGA_OPCODE_H_

#include <uapi/compiler.h>
#include <uapi/net_header.h>

#define PROC_NAME_LEN		(64)
#define BOARD_NAME_LEN		(64)

enum LEGOFPGA_OPCODE_REQ {
	OP_REQ_TEST = 0,

	OP_REQ_ALLOC,
	OP_REQ_ALLOC_RESP,
	OP_REQ_FREE,
	OP_REQ_FREE_RESP,

	OP_REQ_READ,
	OP_REQ_READ_RESP,
	OP_REQ_WRITE,
	OP_REQ_WRITE_RESP,

	OP_CREATE_PROC,
	OP_CREATE_PROC_RESP,
	OP_FREE_PROC,
	OP_FREE_PROC_RESP,

	OP_OPEN_SESSION,
	OP_OPEN_SESSION_RESP,
	OP_CLOSE_SESSION,
	OP_CLOSE_SESSION_RESP,

	/* Migration */
	OP_REQ_MIGRATION_H2M,
	OP_REQ_MIGRATION_H2M_RESP,
	OP_REQ_MIGRATION_B2M,
	OP_REQ_MIGRATION_B2M_RESP,
	OP_REQ_MIGRATION_M2B_RECV,	/* new board, prepare for a incoming mig */
	OP_REQ_MIGRATION_M2B_RECV_RESP,
	OP_REQ_MIGRATION_M2B_RECV_CANCEL,
	OP_REQ_MIGRATION_M2B_RECV_CANCEL_RESP,
	OP_REQ_MIGRATION_M2B_SEND,	/* old board, start migrate to new board */
	OP_REQ_MIGRATION_M2B_SEND_RESP,

	/* Host to Monitor */
	OP_REQ_MEMBERSHIP_JOIN_CLUSTER,
	OP_REQ_MEMBERSHIP_JOIN_CLUSTER_RESP,

	OP_REQ_MEMBERSHIP_NEW_NODE,
	OP_REQ_MEMBERSHIP_NEW_NODE_RESP,

	/*
	 * Query stats
	 */
	OP_REQ_QUERY_STAT,
	OP_REQ_QUERY_STAT_RESP,

	OP_RESET_ALL,

	OP_REQ_SOC_DEBUG,

	/*
	 * For host and monitor, this is a normal pingpong.
	 * For legomem-board, this is undefined.
	 */
	OP_REQ_PINGPONG,

	/*
	 * For legomem-board, this pingpong msg should
	 * return at the point of reaching network stack
	 */
	OP_REQ_FPGA_PINGPONG,

	/*
	 * For legomem-board, this pingpoong msg shoul
	 * return at the point of reaching SoC
	 */
	OP_REQ_SOC_PINGPONG,
};

static inline char *legomem_opcode_str(unsigned int opcode)
{
#define S(_OP) \
	case _OP:				return __stringify(_OP)

	switch (opcode) {
	S(OP_REQ_TEST);
	S(OP_REQ_ALLOC);
	S(OP_REQ_ALLOC_RESP);
	case OP_REQ_FREE:			return "op_free";
	case OP_REQ_FREE_RESP:			return "op_free_resp";
	case OP_REQ_READ:			return "op_read";
	case OP_REQ_READ_RESP:			return "op_read_resp";
	case OP_REQ_WRITE:			return "op_write";
	case OP_REQ_WRITE_RESP:			return "op_write_resp";
	case OP_CREATE_PROC:			return "op_create_proc";
	case OP_CREATE_PROC_RESP:		return "op_create_proc_resp";
	case OP_FREE_PROC:			return "op_free_proc";
	case OP_FREE_PROC_RESP:			return "op_free_proc_resp";
	case OP_REQ_MIGRATION_H2M:		return "op_migration_h2m";
	case OP_REQ_MIGRATION_H2M_RESP:		return "op_migration_h2m_resp";
	case OP_REQ_MIGRATION_B2M:		return "op_migration_b2m";
	case OP_REQ_MIGRATION_B2M_RESP:		return "op_migration_b2m_resp";
	case OP_REQ_MIGRATION_M2B_RECV:		return "op_migration_m2b_recv";
	S(OP_REQ_MIGRATION_M2B_RECV_RESP);
	S(OP_REQ_MIGRATION_M2B_RECV_CANCEL_RESP);
	S(OP_REQ_MIGRATION_M2B_SEND);
	S(OP_REQ_MIGRATION_M2B_SEND_RESP);
	S(OP_OPEN_SESSION);
	S(OP_OPEN_SESSION_RESP);
	S(OP_CLOSE_SESSION);
	S(OP_CLOSE_SESSION_RESP);
	S(OP_REQ_MEMBERSHIP_JOIN_CLUSTER);
	S(OP_REQ_MEMBERSHIP_JOIN_CLUSTER_RESP);
	S(OP_REQ_MEMBERSHIP_NEW_NODE);
	S(OP_REQ_MEMBERSHIP_NEW_NODE_RESP);
	S(OP_REQ_QUERY_STAT);
	S(OP_REQ_QUERY_STAT_RESP);
	S(OP_REQ_PINGPONG);
	S(OP_REQ_FPGA_PINGPONG);
	S(OP_REQ_SOC_PINGPONG);
	default:				return "unknown";
	};
	return NULL;
}

/*
 * For all op structures, their position within packet are fixed.
 * They are placed right after struct lego_hdr.
 *
 * -------------------------------------------------------
 * ^     ^       ^     ^         ^          ^       ^
 * | ETH |  IP   | UDP |  GBN    | LegoMem | OP_XXX   | data  |
 */

struct op_membership_join_cluster {
	/*
	 * We reuse the BOARD_INFO_FLAGS_XXX to indicate
	 * the requester's node type.
	 */
	unsigned long type;
	unsigned long mem_size_bytes;
	struct endpoint_info ei;
} __packed;

struct op_membership_new_node {
	unsigned long type;
	unsigned long mem_size_bytes;

	/*
	 * the endhost name is uniquely constructed
	 * by monitor and is globally visible
	 */
	char name[BOARD_NAME_LEN];
	struct endpoint_info ei;
} __packed;

struct op_alloc_free {
	unsigned long	addr;
	unsigned long	len;
	unsigned long	vregion_idx;
	unsigned long	vm_flags;
} __packed;

struct op_alloc_free_ret {
	unsigned int 	ret;
	unsigned long	addr;

	unsigned int	board_ip, udp_port, vregion_idx;
} __packed;

struct op_create_proc {
	char proc_name[PROC_NAME_LEN];
} __packed;

struct op_create_proc_resp {
	int ret;
	pid_t pid;
} __packed;

/*
 * For
 * - OP_REQ_READ
 * - OP_REQ_WRITE
 */
struct op_read_write {
	unsigned long __remote	va;
	unsigned long		size;

	/* Hold write data, variable length */
	char			data[0];
} __packed;

struct op_read_write_ret {
	unsigned char		ret;

	/* Hold read read, variable length */
	char			data[0];
} __packed;

/*
 * For open and close network sessions
 * - OP_OPEN_SESSION
 * - OP_CLOSE_SESSION
 */
struct op_open_close_session {
	/*
	 * For OPEN, this is the src session id
	 * For CLOSE, this is the intended session
	 */
	unsigned int	session_id;
};

struct op_open_close_session_ret {
	/*
	 * For OPEN, this is the dst session id.
	 * For CLOSE, this is the status.
	 */
	unsigned int	session_id;
};

struct op_migration {
	unsigned int src_board_ip, src_udp_port;
	unsigned int dst_board_ip, dst_udp_port;
	unsigned int vregion_index;
};

struct op_migration_ret {
	int ret;
};

struct legomem_common_headers {
	struct eth_hdr		eth;
	struct ipv4_hdr		ipv4;
	struct udp_hdr		udp;
	struct gbn_header	gbn;
	struct lego_header	lego;
} __packed;

/*
 * Define a whole msg
 */

/*
 * Create and close contexts
 */
struct legomem_create_context_req {
	struct legomem_common_headers comm_headers;
	struct op_create_proc op;
} __packed;
struct legomem_create_context_resp {
	struct legomem_common_headers comm_headers;
	struct op_create_proc_resp op;
} __packed;

struct legomem_close_context_req {
	struct legomem_common_headers comm_headers;
} __packed;
struct legomem_close_context_resp {
	struct legomem_common_headers comm_headers;
	int ret;
} __packed;

/*
 * Read and Write
 */
struct legomem_read_write_req {
	struct legomem_common_headers comm_headers;
	struct op_read_write op;
} __packed;
struct legomem_read_write_resp {
	struct legomem_common_headers comm_headers;
	struct op_read_write_ret ret;
} __packed;

/*
 * Alloc and Free
 */
struct legomem_alloc_free_req {
	struct legomem_common_headers comm_headers;
	struct op_alloc_free op;
} __packed;
struct legomem_alloc_free_resp {
	struct legomem_common_headers comm_headers;
	struct op_alloc_free_ret op;
} __packed;

/*
 * Open and close a network session
 */
struct legomem_open_close_session_req {
	struct legomem_common_headers comm_headers;
	struct op_open_close_session op;
} __packed;
struct legomem_open_close_session_resp {
	struct legomem_common_headers comm_headers;
	struct op_open_close_session_ret op;
} __packed;

/*
 * Membership
 */
struct legomem_membership_join_cluster_req {
	struct legomem_common_headers comm_headers;
	struct op_membership_join_cluster op;
} __packed;

struct legomem_membership_join_cluster_resp {
	struct legomem_common_headers comm_headers;
	int ret;
} __packed;

struct legomem_membership_new_node_req {
	struct legomem_common_headers comm_headers;
	struct op_membership_new_node op;
} __packed;

struct legomem_membership_new_node_resp {
	struct legomem_common_headers comm_headers;
	int ret;
} __packed;

/*
 * Migration
 */
struct legomem_migration_req {
	struct legomem_common_headers comm_headers;
	struct op_migration op;
} __packed;
struct legomem_migration_resp {
	struct legomem_common_headers comm_headers;
	struct op_migration_ret op;
} __packed;

/*
 * Debug and Measurement
 */
struct legomem_pingpong_req {
	struct legomem_common_headers comm_headers;
} __packed;

struct legomem_pingpong_resp {
	struct legomem_common_headers comm_headers;
} __packed;

struct legomem_query_stat_req {
	struct legomem_common_headers comm_headers;
} __packed;

struct legomem_query_stat_resp {
	struct legomem_common_headers comm_headers;
	unsigned int nr_items;
	unsigned long *stat;
} __packed;

#endif /* _LEGOFPGA_OPCODE_H_ */
