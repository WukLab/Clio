/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_EXT_EP_H_
#define _LEGO_MEM_EXT_EP_H_

#include <stddef.h>
#include <uapi/lego_mem.h>
#include <fpga/lego_mem_ctrl.h>

/* TODO: dummy address used for xbar routing */
#define SOC_XBAR_EPID			0
#define SOC_XBAR_ADDR			0
#define EXTAPI_XBAR_EPID		1
#define EXTAPI_XBAR_ADDR		1
#define VM_PRIVATE			0
#define VM_SHARE			1

/**
 * module will not work if data width is less than header size
 * SIZE:  number in bytes
 * WIDTH: number in bits
 */
#define DATAWIDTH			512
#define DATASIZE			((DATAWIDTH) >> 3)
#define BYTETOBIT(byte)			((byte) << 3)
#define COREMEMDATASIZE			(DATASIZE - sizeof(struct legomem_rw_fpgamsg))
#define COREMEMRETDATASIZE		(DATASIZE - sizeof(struct legomem_rw_fpgamsg_resp))

/* external interface */
struct data_if {
	ap_uint<DATAWIDTH>	pkt;
	ap_uint<1> 		last;
};

struct ctrl_if {
	struct lego_mem_ctrl	pkt;
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

#define boundary1(name, struct1, field)				\
static const unsigned long name##_##field##_lo 			\
	= lowbound(struct1, field);				\
static const unsigned long name##_##field##_up			\
	= upbound(name##_##field##_lo, struct1, field);

#define boundary2(name, struct1, field1, struct2, field2)	\
static const unsigned long name##_##field2##_lo 		\
	= lowbound2(struct1, field1, struct2, field2);		\
static const unsigned long name##_##field2##_up			\
	= upbound(name##_##field2##_lo, struct2, field2);

#define field(data, fieldname)	\
	data.range(fieldname ## _up, fieldname ## _lo)

// header field
boundary1(hdr, lego_header, pid)
boundary1(hdr, lego_header, opcode)
boundary1(hdr, lego_header, seqId)
boundary1(hdr, lego_header, size)
boundary1(hdr, lego_header, cont)
// manual
#define hdr_lo			0
#define hdr_up			(BYTETOBIT(sizeof(struct lego_header)) - 1)
#define hdr_req_status_lo	32
#define hdr_req_status_up	(hdr_req_status_lo + 3)

// coremem field
boundary2(mem, legomem_rw_fpgamsg, op, op_read_write, va)
boundary2(mem, legomem_rw_fpgamsg, op, op_read_write, size)

// alloc free field
boundary2(malloc, legomem_alloc_free_fpgamsg, op, op_alloc_free, addr)
boundary2(malloc, legomem_alloc_free_fpgamsg, op, op_alloc_free, len)
boundary2(malloc, legomem_alloc_free_fpgamsg, op, op_alloc_free, vregion_idx)
boundary2(malloc, legomem_alloc_free_fpgamsg, op, op_alloc_free, vm_flags)
boundary2(mallocret, legomem_alloc_free_fpgamsg, op, op_alloc_free_ret, ret)
boundary2(mallocret, legomem_alloc_free_fpgamsg, op, op_alloc_free_ret, addr)

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

#define release_delay_pkt(ports, pkt)	\
do {					\
	if (pkt.vld) {			\
		ports.write(pkt.data);	\
		pkt.vld = 0;		\
	}				\
} while(0)

#endif /* _LEGO_MEM_EXT_EP_H_ */
