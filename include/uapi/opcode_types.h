/*
 * Copyright (c) 2020，Wuklab, UCSD.
 *
 * This file defines the request opcodes used in LegoMem requests.
 * This file is used by both host, FPGA, and SoC.
 */

#ifndef _LEGOFPGA_OPCODE_TYPES_H_
#define _LEGOFPGA_OPCODE_TYPES_H_

#define PROC_NAME_LEN		(64)
#define BOARD_NAME_LEN		(64)

enum LEGOFPGA_OPCODE_REQ {
	OP_REQ_INVALID = 0,
	OP_REQ_TEST,

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

#endif /* _LEGOFPGA_OPCODE_TYPES_H_ */
