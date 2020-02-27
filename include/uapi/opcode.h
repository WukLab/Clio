/*
 * This file defines the request opcodes used in LegoMem requests.
 * This file is used by both host, FPGA, and SoC.
 */

#ifndef _LEGOFPGA_OPCODE_H_
#define _LEGOFPGA_OPCODE_H_

enum LEGOFPGA_OPCODE_REQ {
	OP_REQ_ALLOC = 1,
	OP_REQ_FREE,

	OP_REQ_READ,
	OP_REQ_WRITE,

	OP_CREATE_PROC,
	OP_FREE_PROC,

	OP_REQ_MIGRATION,

	OP_RESET_ALL,

	OP_REQ_SOC_DEBUG,
	OP_REQ_FPGA_PINGPOING,	/* For measurement */
	OP_REQ_SOC_PINGPONG,	/* For measurement */
};

/*
 * For all op structures, their position within packet are fixed.
 * They are placed right after struct lego_hdr.
 *
 * --------------------------------------------------
 * ^     ^       ^     ^         ^          ^       ^
 * | ETH |  IP   | UDP | LegoMem | OP_XXX   | data  |
 */

struct op_alloc_free {
	unsigned int	pid;
	unsigned long	addr;
	unsigned long	len;
	unsigned long	vm_flags;
} __attribute__((packed));

struct op_alloc_free_ret {
	unsigned int 	ret;
	unsigned long	addr;
} __attribute__((packed));

#endif /* _LEGOFPGA_OPCODE_H_ */
