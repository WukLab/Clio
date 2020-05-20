/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_EXT_EP_TYPES_H_
#define _LEGO_MEM_EXT_EP_TYPES_H_

#include <ap_int.h>
#include <uapi/opcode.h>

/* ctrl structure */
/*
 * co-sim don't work on bit-field, so use this structure
 * keep this structure the same as in fpga/lego_mem_ctrl.h
 */
struct lego_mem_ctrl {
	ap_uint<32> param32;
	ap_uint<8> param8;
	ap_uint<4> beats;
	ap_uint<4> cmd;
	ap_uint<8> addr;
	ap_uint<8> epid;
};

// structure used to refer offset, all structure must be less than or equal to 64 bytes, 512 bits

/* core mem struct */
struct legomem_rw_fpgamsg {
	struct lego_header 		hdr;
	struct op_read_write 		op;
} __packed;
struct legomem_rw_fpgamsg_resp {
	struct lego_header 		hdr;
	struct op_read_write_ret 	op;
} __packed;

/* alloc free struct */
struct legomem_alloc_free_fpgamsg {
	struct lego_header 		hdr;
	struct op_alloc_free 		op;
} __packed;
struct legomem_alloc_free_fpgamsg_resp {
	struct lego_header 		hdr;
	struct op_alloc_free_ret 	op;
} __packed;

/* multi-version data store */
// create/delete
struct verobj_create_delete {
	struct lego_header 			hdr;
	struct op_verobj_create_delete 		op;
} __packed;
struct verobj_create_delete_ret {
	struct lego_header 			hdr;
	struct op_verobj_create_delete_ret 	op;
} __packed;

// read/write
struct verobj_read_write {
	struct lego_header 			hdr;
	struct op_verobj_read_write 		op;
} __packed;
struct verobj_read_write_ret {
	struct lego_header 			hdr;
	struct op_verobj_read_write_ret 	op;
} __packed;

#endif /* _LEGO_MEM_EXT_EP_TYPES_H_ */
