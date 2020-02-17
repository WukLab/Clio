/*
 * This file defines the request opcodes used in LegoFPGA requests.
 * This file is used by both host, FPGA, and SoC.
 */

#ifndef _LEGOFPGA_OPCODE_H_
#define _LEGOFPGA_OPCODE_H_

enum LEGOFPGA_OPCODE_REQ {
	OP_REQ_TEST,

	OP_REQ_ALLOC,
	OP_REQ_FREE,

	OP_REQ_READ,
	OP_REQ_WRITE,
};

#endif /* _LEGOFPGA_OPCODE_H_ */
