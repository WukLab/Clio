/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_DEREF_PTR_H_
#define _LEGO_MEM_DEREF_PTR_H_

#include <hls_stream.h>
#include <ap_int.h>
#include <ext_ep.h>


/**
 * interface and configurations
 */
void deref_ptr(hls::stream<ap_uint<DATAWIDTH> > &data_in, hls::stream<ap_uint<DATAWIDTH> > &data_out);


/**
 * dereference pointer fpga internal message structure
 */
struct legomem_deref_fpgamsg {
	struct lego_header 	hdr;
	struct op_deref 	op;
};


/**
 * below are internal used structure
 * SIZE:  number in bytes
 * WIDTH: number in bits
 */
#define DEREFDATASIZE		(DATASIZE - sizeof(struct legomem_deref_fpgamsg))
#define RESPDATASIZE		(DATASIZE - sizeof(struct lego_header))
// assume deref header is larger
#define MISMATCHSIZE		(COREMEMDATASIZE - DEREFDATASIZE)
// complement of mismatch size in terms of interface width
#define MISMATCHSIZECMPL	(DATASIZE - MISMATCHSIZE)


// structure used for refer offset, all structure must be 64 bytes, 512 bits
struct coremem_req_1st {
	struct legomem_rw_fpgamsg 	comm;
	uint8_t				data1[DEREFDATASIZE];
	uint8_t				data2[MISMATCHSIZE];
};

struct coremem_req_rest {
	uint8_t				data1[MISMATCHSIZECMPL];
	uint8_t				data2[MISMATCHSIZE];
};

/* used for read response */
struct coremem_ret {
	struct lego_header		hdr;
	union {
		uint64_t		addr;
		uint8_t			data[RESPDATASIZE];
	};
};

struct deref_req_1st {
	struct legomem_deref_fpgamsg 	comm;
	uint8_t				data[DEREFDATASIZE];
};

struct deref_req_rest {
	uint8_t				data1[MISMATCHSIZE];
	uint8_t				data2[MISMATCHSIZECMPL];
};

// deref pointer field
#define deref_addr_lo		lowbound2(legomem_deref_fpgamsg, op, op_deref, addr)
#define deref_addr_up		upbound(deref_addr_lo, op_deref, addr)
#define deref_off1_lo		lowbound2(legomem_deref_fpgamsg, op, op_deref, off1)
#define deref_off1_up		upbound(deref_off1_lo, op_deref, off1)
#define deref_off2_lo		lowbound2(legomem_deref_fpgamsg, op, op_deref, off2)
#define deref_off2_up		upbound(deref_off2_lo, op_deref, off2)
#define deref_size_lo		lowbound2(legomem_deref_fpgamsg, op, op_deref, size)
#define deref_size_up		upbound(deref_size_lo, op_deref, size)

// data field
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
#define deref_1st_data_lo	lowbound(deref_req_1st, data)
#define deref_1st_data_up	upbound(deref_1st_data_lo, deref_req_1st, data)
#define deref_rest_data1_lo	lowbound(deref_req_rest, data1)
#define deref_rest_data1_up	upbound(deref_rest_data1_lo, deref_req_rest, data1)
#define deref_rest_data2_lo	lowbound(deref_req_rest, data2)
#define deref_rest_data2_up	upbound(deref_rest_data2_lo, deref_req_rest, data2)


/* internal interface */
struct data_if {
	ap_uint<DATAWIDTH>	pkt;
	ap_uint<1> 		last;
};

struct release_if {
	ap_uint<4>  		status;
	ap_uint<64>  		addr;
};


#endif /* _LEGO_MEM_DEREF_PTR_H_ */
