/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_EXT_EP_H_
#define _LEGO_MEM_EXT_EP_H_

#include <hls_stream.h>
#include <ap_int.h>
#include <stddef.h>
#include "ext_ep_api.h"
// TODO: uncomment it during integration
//#include <uapi/opcode.h>
//#include <uapi/lego_mem.h>

/**
 * interface and configurations
 */
// module will not work if data width is less than header size
#define DATAWIDTH		512
void ext_ep(hls::stream<ap_uint<DATAWIDTH> > &data_in, hls::stream<ap_uint<DATAWIDTH> > &data_out);

/**
 * below are internal used structure
 * SIZE:  number in bytes
 * WIDTH: number in bits
 */
#define DATASIZE		((DATAWIDTH) >> 3)
#define BYTETOBIT(byte)		((byte) << 3)
#define DEREFDATASIZE		(DATASIZE - sizeof(struct legomem_deref_req))
#define COREMEMDATASIZE		(DATASIZE - sizeof(struct legomem_read_write_req))
#define RESPDATASIZE		(DATASIZE - sizeof(struct lego_mem_header))
// assume deref header is larger
#define MISMATCHSIZE		(COREMEMDATASIZE - DEREFDATASIZE)
// complement of mismatch size in terms of interface width
#define MISMATCHSIZECMPL	(DATASIZE - MISMATCHSIZE)

// structure used for refer offset, all structure must be 64 bytes, 512 bits
struct deref_req_1st {
	struct legomem_deref_req 	comm;
	uint8_t				data[DEREFDATASIZE];
};

struct deref_req_rest {
	uint8_t				data1[MISMATCHSIZE];
	uint8_t				data2[MISMATCHSIZECMPL];
};

struct coremem_req_1st {
	struct legomem_read_write_req 	comm;
	uint8_t				data1[DEREFDATASIZE];
	uint8_t				data2[MISMATCHSIZE];
};

struct coremem_req_rest {
	uint8_t				data1[MISMATCHSIZECMPL];
	uint8_t				data2[MISMATCHSIZE];
};

/* used for read response */
struct coremem_ret {
	struct lego_mem_header		hdr;
	union {
		uint64_t		addr;
		uint8_t			data[RESPDATASIZE];
	};
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
#define hdr_req_type_lo		lowbound(lego_mem_header, req_type)
#define hdr_req_type_up		upbound(hdr_req_type_lo, lego_mem_header, req_type)
#define hdr_seqId_lo		lowbound(lego_mem_header, seqId)
#define hdr_seqId_up		upbound(hdr_seqId_lo, lego_mem_header, seqId)
#define hdr_size_lo		lowbound(lego_mem_header, size)
#define hdr_size_up		upbound(hdr_size_lo, lego_mem_header, size)
#define hdr_cont_lo		lowbound(lego_mem_header, cont)
#define hdr_cont_up		upbound(hdr_cont_lo, lego_mem_header, cont)
// manual
#define hdr_lo			0
#define hdr_up			(BYTETOBIT(sizeof(struct lego_mem_header)) - 1)
#define hdr_req_status_lo	32
#define hdr_req_status_up	(hdr_req_status_lo + 3)
#define hdr_access_cnt_bit	38

// coremem field
#define mem_va_lo		lowbound2(legomem_read_write_req, op, op_read_write, va)
#define mem_va_up		upbound(mem_va_lo, op_read_write, va)
#define mem_size_lo		lowbound2(legomem_read_write_req, op, op_read_write, size)
#define mem_size_up		upbound(mem_size_lo, op_read_write, size)

// deref pointer field
#define deref_addr_lo		lowbound2(legomem_deref_req, op, op_deref, addr)
#define deref_addr_up		upbound(deref_addr_lo, op_deref, addr)
#define deref_off1_lo		lowbound2(legomem_deref_req, op, op_deref, off1)
#define deref_off1_up		upbound(deref_off1_lo, op_deref, off1)
#define deref_off2_lo		lowbound2(legomem_deref_req, op, op_deref, off2)
#define deref_off2_up		upbound(deref_off2_lo, op_deref, off2)
#define deref_size_lo		lowbound2(legomem_deref_req, op, op_deref, size)
#define deref_size_up		upbound(deref_size_lo, op_deref, size)

// data field
#define deref_1st_data_lo	lowbound(deref_req_1st, data)
#define deref_1st_data_up	upbound(deref_1st_data_lo, deref_req_1st, data)
#define deref_rest_data1_lo	lowbound(deref_req_rest, data1)
#define deref_rest_data1_up	upbound(deref_rest_data1_lo, deref_req_rest, data1)
#define deref_rest_data2_lo	lowbound(deref_req_rest, data2)
#define deref_rest_data2_up	upbound(deref_rest_data2_lo, deref_req_rest, data2)
#define mem_1st_data1_lo	lowbound(coremem_req_1st, data1)
#define mem_1st_data1_up	upbound(mem_1st_data1_lo, coremem_req_1st, data1)
#define mem_1st_data2_lo	lowbound(coremem_req_1st, data2)
#define mem_1st_data2_up	upbound(mem_1st_data2_lo, coremem_req_1st, data2)
#define mem_rest_data1_lo	lowbound(coremem_req_rest, data1)
#define mem_rest_data1_up	upbound(mem_rest_data1_lo, coremem_req_rest, data1)
#define mem_rest_data2_lo	mem_1st_data2_lo
#define mem_rest_data2_up	mem_1st_data2_up
#define mem_ret_addr_lo		lowbound(coremem_ret, addr)
#define mem_ret_addr_up		upbound(mem_ret_addr_lo, coremem_ret, addr)


/* internal interface */
struct data_if {
	ap_uint<DATAWIDTH>	pkt;
	ap_uint<1> 		last;
};

struct release_if {
	ap_uint<4>  		status;
	ap_uint<64>  		addr;
};

template <typename T>
struct delay {
	T			data;
	ap_uint<1>		vld;
};

#define delay_pkt(pipe, input)	\
do {				\
	pipe.data = input;	\
	pipe.vld = 1;		\
} while(0)

template <typename T>
class FIFO {

protected:
	T top;
	uint8_t top_vld;
	hls::stream<T> rest;

public:
	FIFO() {
		top = 0;
		top_vld = 0;
	}

	bool empty() {
		return top_vld == 0;
	}

	T front() {
		if (top_vld == 0)
			return static_cast<T>(0);

		return top;
	}

	// assume user check empty before pop
	T pop() {
		T top_entry = top;
		if (!rest.empty())
			top = rest.read();
		else
			top_vld = 0;
		return top_entry;
	}

	void push(T input) {
		if (empty()) {
			top = input;
			top_vld = 1;
		} else {
			rest.write(input);
		}
	}
};


#endif /* _LEGO_MEM_EXT_EP_H_ */
