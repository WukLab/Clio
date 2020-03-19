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

	OP_REQ_ALLOC = 1,
	OP_REQ_FREE,

	OP_REQ_READ,
	OP_REQ_WRITE,

	OP_CREATE_PROC,
	OP_FREE_PROC,

	OP_OPEN_SESSION,
	OP_CLOSE_SESSION,

	OP_REQ_MIGRATION,

	/* Host to Monitor */
	OP_REQ_MEMBERSHIP_JOIN_CLUSTER,

	/* Monitor to Host */
	OP_REQ_MEMBERSHIP_NEW_BOARD,

	OP_RESET_ALL,

	OP_REQ_SOC_DEBUG,
	OP_REQ_FPGA_PINGPOING,	/* For measurement */
	OP_REQ_SOC_PINGPONG,	/* For measurement */
};

/*
 * For all op structures, their position within packet are fixed.
 * They are placed right after struct lego_hdr.
 *
 * -------------------------------------------------------
 * ^     ^       ^     ^         ^          ^       ^
 * | ETH |  IP   | UDP |  GBN    | LegoMem | OP_XXX   | data  |
 */

struct op_membership_join_cluster {
	struct endpoint_info ei;
} __packed;

struct op_membership_new_board {
	/*
	 * this is a variable-sized array
	 * size = cnt * sizeof(struct endpoint_info) + sizeof(cnt);
	 */
	int cnt;
	struct endpoint_info *ei_array;
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

	unsigned int	board_ip, vregion_idx;
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
	 * For OPEN, this is not used.
	 * For CLOSE, this is the intended session
	 */
	unsigned int	session_id;
};

struct op_open_close_session_ret {
	/* Opposite of the above definitions */
	unsigned int	session_id;
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
};
struct legomem_create_context_resp {
	struct legomem_common_headers comm_headers;
	struct op_create_proc_resp op;
};

struct legomem_close_context_req {
	struct legomem_common_headers comm_headers;
};
struct legomem_close_context_resp {
	struct legomem_common_headers comm_headers;
	int ret;
};

/*
 * Read and Write
 */
struct legomem_read_write_req {
	struct legomem_common_headers comm_headers;
	struct op_read_write op;
};
struct legomem_read_write_resp {
	struct legomem_common_headers comm_headers;
	struct op_read_write_ret ret;
};

/*
 * Alloc and Free
 */
struct legomem_alloc_free_req {
	struct legomem_common_headers comm_headers;
	struct op_alloc_free op;
};
struct legomem_alloc_free_resp {
	struct legomem_common_headers comm_headers;
	struct op_alloc_free_ret op;
};

/*
 * Open and close a network session
 */
struct legomem_open_close_session_req {
	struct legomem_common_headers comm_headers;
	struct op_open_close_session op;
};
struct legomem_open_close_session_resp {
	struct legomem_common_headers comm_headers;
	struct op_open_close_session_ret op;
};

/*
 * Membership
 */
struct legomem_membership_join_cluster_req {
	struct legomem_common_headers comm_headers;
	struct op_membership_join_cluster op;
};

struct legomem_membership_join_cluster_resp {
	struct legomem_common_headers comm_headers;
	int ret;
};

struct legomem_membership_new_board_req {
	struct legomem_common_headers comm_headers;
	struct op_membership_new_board op;
};

struct legomem_membership_new_board_resp {
	struct legomem_common_headers comm_headers;
	int ret;
};

#endif /* _LEGOFPGA_OPCODE_H_ */
