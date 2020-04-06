/*
 * Copyright (c) 2020，Wuklab, UCSD.
 *
 * This file defines everything about LegoMem Opcodes and its message formats.
 * The opcode definitions are in opcode_types.h, which is a generic file that
 * could be usef by fpga as well. This file can be included by host only.
 */

#ifndef _LEGOFPGA_OPCODE_H_
#define _LEGOFPGA_OPCODE_H_

#include <uapi/stat.h>
#include <uapi/compiler.h>
#include <uapi/net_header.h>
#include <uapi/opcode_types.h>	/* Opcode definitions */

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

static inline size_t legomem_query_stat_resp_size(void)
{
	return sizeof(struct legomem_query_stat_resp) +
	       (NR_STAT_TYPES - 1) * sizeof(unsigned long);
}

#endif /* _LEGOFPGA_OPCODE_H_ */
