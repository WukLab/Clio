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
	/* Zone 0x00 - 0x3F system ops */
	OP_REQ_INVALID = 0,
	OP_REQ_TEST,
	/*
	 * For host and monitor, this is a normal pingpong.
	 * For legomem-board, a PingPong module need to be added for this to work
	 */
	OP_REQ_PINGPONG,
	OP_REQ_BARRIER,

	/* Zone 0x40 - 0x7F, application Ops */
	OP_REQ_READ = 0x40,
	OP_REQ_READ_RESP,

	OP_REQ_WRITE = 0x50,
	OP_REQ_WRITE_RESP,
	OP_REQ_WRITE_NOREPLY,

	OP_REQ_CAHCE_INVALID = 0x70,

	/* Zone 0x80 + */
	OP_REQ_ALLOC = 0x80, 
	OP_REQ_ALLOC_RESP,
	OP_REQ_FREE,
	OP_REQ_FREE_RESP,

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

#endif /* _LEGOFPGA_OPCODE_TYPES_H_ */
