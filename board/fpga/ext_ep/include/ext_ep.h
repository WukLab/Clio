/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_EXT_EP_H_
#define _LEGO_MEM_EXT_EP_H_

#include <stddef.h>
#include <uapi/lego_mem.h>


/**
 * module will not work if data width is less than header size
 * SIZE:  number in bytes
 * WIDTH: number in bits
 */
#define DATAWIDTH		512
#define DATASIZE		((DATAWIDTH) >> 3)
#define BYTETOBIT(byte)		((byte) << 3)


/**
 * common fpga internal message structure
 */
struct legomem_rw_fpgamsg {
	struct lego_header 	hdr;
	struct op_read_write 	op;
};


/**
 * naming scheme:
 * [top struct name]_[target_member_name]_[lo/up]
 *
 * note: lowbound and upbound only works for byte members,
 * bit field can only be done manually
 */
#define lowbound(type, member)	\
	BYTETOBIT(offsetof(struct type, member))
#define lowbound2(type1, member1, type2, member2) \
	(lowbound(type1, member1) + lowbound(type2, member2))
#define upbound(lowbound, type, member) \
	(lowbound - 1 + BYTETOBIT(sizeof(((struct type *)0)->member)))


// header field
#define hdr_opcode_lo		lowbound(lego_header, opcode)
#define hdr_opcode_up		upbound(hdr_opcode_lo, lego_header, opcode)
#define hdr_seqId_lo		lowbound(lego_header, seqId)
#define hdr_seqId_up		upbound(hdr_seqId_lo, lego_header, seqId)
#define hdr_size_lo		lowbound(lego_header, size)
#define hdr_size_up		upbound(hdr_size_lo, lego_header, size)
#define hdr_cont_lo		lowbound(lego_header, cont)
#define hdr_cont_up		upbound(hdr_cont_lo, lego_header, cont)
// manual
#define hdr_lo			0
#define hdr_up			(BYTETOBIT(sizeof(struct lego_header)) - 1)
#define hdr_req_status_lo	32
#define hdr_req_status_up	(hdr_req_status_lo + 3)


/* internal interface */
template <typename T>
struct delay {
	T			data;
	bool			vld;
};

#define delay_pkt(pipe, input)	\
do {				\
	pipe.data = input;	\
	pipe.vld = 1;		\
} while(0)


#endif /* _LEGO_MEM_EXT_EP_H_ */
