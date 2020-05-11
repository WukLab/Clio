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

static inline char *legomem_opcode_str(unsigned int opcode)
{
#define S(_OP) \
	case _OP:				return __stringify(_OP)

	switch (opcode) {
	S(OP_REQ_INVALID);
	S(OP_REQ_TEST);

	S(OP_REQ_PINGPONG);
	S(OP_REQ_BARRIER);

	S(OP_REQ_READ);
	S(OP_REQ_READ_RESP);

	S(OP_REQ_WRITE);
	S(OP_REQ_WRITE_RESP);
	S(OP_REQ_WRITE_NOREPLY);

	S(OP_REQ_CACHE_INVALID);

	S(OP_REQ_ALLOC);
	S(OP_REQ_ALLOC_RESP);
	S(OP_REQ_FREE);
	S(OP_REQ_FREE_RESP);

	S(OP_CREATE_PROC);
	S(OP_CREATE_PROC_RESP);
	S(OP_FREE_PROC);
	S(OP_FREE_PROC_RESP);

	S(OP_REQ_MIGRATION_H2M);
	S(OP_REQ_MIGRATION_H2M_RESP);
	S(OP_REQ_MIGRATION_B2M);
	S(OP_REQ_MIGRATION_B2M_RESP);
	S(OP_REQ_MIGRATION_M2B_RECV);
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
	S(OP_REQ_FPGA_PINGPONG);
	S(OP_REQ_SOC_PINGPONG);
	S(OP_REQ_SOC_PINGPONG_RESP);
	S(OP_REQ_TEST_PTE);
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

#define MAX_LEGOMEM_OP_SIZE	512

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

/*
 * Flags used during legomem_alloc
 * 1. Write: if this new range is writable.
 * 2. Populate: if we should pre-populate pgtables during alloc
 * 3. Zero: if we need to zero out all pages for initial setup
 */
#define LEGOMEM_VM_FLAGS_WRITE		(0x1)
#define LEGOMEM_VM_FLAGS_POPULATE	(0x2)
#define LEGOMEM_VM_FLAGS_ZERO		(0x4)
#define LEGOMEM_VM_FLAGS_CONFLICT	(0x8) /* internal */

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
	unsigned int		size;

	/* Hold write data, variable length */
	char			data[0];
} __packed;

struct op_read_write_ret {
	unsigned long __remote	va;
	unsigned int		size;

	/* Hold read read, variable length */
	char			data[0];
} __packed;

/*
 * BRAM cached pgtable flush a.k.a TLB flush
 * Same as op_read_write.
 */
struct op_cache_flush {
	unsigned long __remote	va;
	unsigned int		size;
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
} __packed;

struct op_open_close_session_ret {
	/*
	 * For OPEN, this is the dst session id.
	 * For CLOSE, this is the status.
	 */
	unsigned int	session_id;
} __packed;

struct op_migration {
	unsigned int src_board_ip, src_udp_port;
	unsigned int dst_board_ip, dst_udp_port;
	unsigned int vregion_index;
} __packed;

struct op_migration_ret {
	int ret;
} __packed;

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
	int reply_size;
} __packed;

struct legomem_pingpong_resp {
	struct legomem_common_headers comm_headers;
	char data[0];
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

/*
 * TEST PTE
 */
#define OP_TEST_PTE_ALLOC	(0)
#define OP_TEST_PTE_FREE	(1)
struct op_test_pte {
	int op;
	pid_t pid;
	unsigned long start;
	unsigned long end;
};
struct legomem_test_pte {
	struct legomem_common_headers comm_headers;
	struct op_test_pte op;
};

#endif /* _LEGOFPGA_OPCODE_H_ */
