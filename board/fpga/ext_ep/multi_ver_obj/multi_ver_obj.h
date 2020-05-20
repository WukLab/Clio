/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_MULTI_VER_OBJ_H_
#define _LEGO_MEM_MULTI_VER_OBJ_H_

#include <hls_stream.h>
#include <ap_int.h>
#include <uapi/log2.h>
#include <fpga/compiler.h>
#include <ext_ep_types.h>
#include <ext_ep.h>
#include <assert.h>
#include "multi_ver_obj_config.h"

/* internal interface */
struct data_if {
	ap_uint<DATAWIDTH>	pkt;
	ap_uint<1> 		last;
};

enum obj_op {
	OBJ_CREATE,
	OBJ_DELETE,
	OBJ_READ,
	OBJ_WRITE,
};
struct wait_if {
	uint8_t			type;
	ap_uint<DATAWIDTH>	pkt;
	ap_uint<1> 		last;
};

enum {
	VERSION_READ,
	VERSION_WRITE,
	VERSION_WRITE_DONE,
	VERSION_INC
};
struct version_bram_if {
	ap_uint<2>		rw;
	ap_uint<32> 		obj_id;
	ap_uint<16> 		version;
};

enum component {
	DROP,
	PARSER,
	WQ1,
	WQ2,
	SQ
};
enum endpoint {
	EP_MEM,
	EP_SOC
};
struct data_record_if {
	ap_uint<DATAWIDTH>	pkt;
	ap_uint<1> 		last;
	ap_uint<8>		opcode;
	ap_uint<3>		dest_comp;	// filled with enum component
	ap_uint<2>		endpoint;	// filled with enum endpoint
};
struct record_out_if {
	ap_uint<8>		opcode;
	ap_uint<3>		dest_comp;	// filled with enum component
	ap_uint<2>		endpoint;	// filled with enum endpoint
};

void multi_ver_obj1(hls::stream<struct lego_mem_ctrl> &ctrl_in, hls::stream<struct lego_mem_ctrl> &ctrl_out,
		hls::stream<ap_uint<DATAWIDTH> > &data_in, hls::stream<ap_uint<DATAWIDTH> > &data_out);

void multi_ver_obj2(hls::stream<struct lego_mem_ctrl> &ctrl_in, hls::stream<struct lego_mem_ctrl> &ctrl_out,
		hls::stream<ap_uint<DATAWIDTH> > &data_in, hls::stream<ap_uint<DATAWIDTH> > &data_out);

void dispatcher(hls::stream<ap_uint<DATAWIDTH> > &data_in,
		hls::stream<struct lego_mem_ctrl> &ctrl_in, hls::stream<struct lego_mem_ctrl> &ctrl_out,
		hls::stream<struct record_out_if> &soc_records, hls::stream<struct record_out_if> &mem_records,
		hls::stream<struct data_if> &to_parser, hls::stream<struct data_if> &to_wq1,
		hls::stream<struct data_if> &to_wq2, hls::stream<struct data_if> &to_sq);

void parser(hls::stream<struct data_if> &data_in, hls::stream<struct version_bram_if> &to_bram,
	    hls::stream<struct data_record_if> &tosq1, hls::stream<struct data_record_if> &tosq2,
	    hls::stream<struct wait_if> &towq);

void version_idxs(hls::stream<struct version_bram_if> &in, hls::stream<struct version_bram_if> &out);
void version_idxs2(hls::stream<struct version_bram_if> &in, hls::stream<struct version_bram_if> &out);

void waitqueue1(hls::stream<struct wait_if> &from_parser, hls::stream<struct wait_if> &towq2,
		hls::stream<struct data_if> &tosq_net,
		hls::stream<struct data_record_if> &tosq_soc, hls::stream<struct data_record_if> &tosq_mem,
		hls::stream<struct data_if> &from_disp, hls::stream<struct version_bram_if> &from_ver);

void waitqueue2(hls::stream<struct wait_if> &from_wq1, hls::stream<struct data_if> &from_disp,
		hls::stream<struct data_if> &tosq_net, hls::stream<struct data_record_if> &tosq_mem);

void sendqueue(hls::stream<struct data_record_if> &from_parser1, hls::stream<struct data_record_if> &from_parser2,
	       hls::stream<struct data_if> &from_wq1_net, hls::stream<struct data_record_if> &from_wq1_soc,
	       hls::stream<struct data_record_if> &from_wq1_mem, hls::stream<struct data_if> &from_wq2_net,
	       hls::stream<struct data_record_if> &from_wq2_mem, hls::stream<struct data_if> &from_dispatch,
	       hls::stream<struct record_out_if> &soc_wip, hls::stream<struct record_out_if> &mem_wip,
	       hls::stream<ap_uint<DATAWIDTH> > &data_out);


/* flags */
#define VEROBJ_PRIVATE		0
#define VEROBJ_SHARE		1

struct obj_array_metadata {
	uint8_t			vld;
	uint8_t			flags;
	uint16_t		pid;
	uint16_t		size;
	uint16_t		unused;		// not necessary
	uint64_t		objptr;
} __packed;
#define LOG_VERSION_ARRAY_COUNT	(ilog2(VERSION_ARRAY_COUNT))
#define OBJ_METADATA_SIZE	sizeof(struct obj_array_metadata)
#define LOG_OBJ_METADATA_SIZE	(ilog2(OBJ_METADATA_SIZE))
#define OBJ_ARRAY_SIZE		(OBJ_ARRAY_COUNT * OBJ_METADATA_SIZE)
#define IDX_SIZE		(4)
#define LOG_IDX_SIZE		(ilog2(IDX_SIZE))
#define FREELIST_SIZE		(OBJ_ARRAY_COUNT * IDX_SIZE)

/*
 * compile time configuration check
 * Also checking all the assumption made
 */
#define IS_POWER_OF_TWO(n)	((n) != 0 && (((n) & ((n) - 1)) == 0))
STATIC_ASSERT(IS_POWER_OF_TWO(VERSION_ARRAY_COUNT),
	"VERSION_ARRAY_COUNT need to be power of 2 to be able to use shift operation");
STATIC_ASSERT(IS_POWER_OF_TWO(OBJ_ARRAY_COUNT),
	"OBJ_ARRAY_COUNT need to be power of 2 to be able to use shift operation");
STATIC_ASSERT(IS_POWER_OF_TWO(IDX_SIZE),
	"IDX_SIZE need to be power of 2 to be able to use shift operation");
STATIC_ASSERT(IS_POWER_OF_TWO(OBJ_METADATA_SIZE),
	"sizeof(struct obj_array_metadata) need to be power of 2 to be able to use shift operation");
STATIC_ASSERT(OBJ_METADATA_SIZE + sizeof(struct lego_header) <= DATASIZE,
	"this guarantee decoder get metadata in 1 cycle, wrong behavior if violating this condition");
STATIC_ASSERT(sizeof(struct op_verobj_read_write) == sizeof(struct op_read_write),
	"This design requires these two structures have same size to ignore data frame mis-alignment");
STATIC_ASSERT(sizeof(struct op_verobj_read_write_ret) == sizeof(struct op_read_write_ret),
	"This design requires these two structures have same size to ignore data frame mis-alignment");
STATIC_ASSERT(FIELD_SIZEOF(version_bram_if, obj_id) == IDX_SIZE,
	"type of obj_id must be consistent with IDX_SIZE");


// multi version object field
// create/delete
boundary2(verobjcd, verobj_create_delete, op, op_verobj_create_delete, obj_size_id)
boundary2(verobjcd, verobj_create_delete, op, op_verobj_create_delete, vregion_idx)
boundary2(verobjcd, verobj_create_delete, op, op_verobj_create_delete, vm_flags)
boundary2(verobjcdret, verobj_create_delete, op, op_verobj_create_delete_ret, obj_id)

// read/write
boundary2(verobjrw, verobj_read_write, op, op_verobj_read_write, obj_id)
boundary2(verobjrw, verobj_read_write, op, op_verobj_read_write, version)

// freelist field
struct verobj_freelist_rw {
	struct legomem_rw_fpgamsg		unused;
	uint32_t				obj_id;
} __packed;

struct verobj_freelist_rw_ret {
	struct legomem_rw_fpgamsg_resp		unused;
	uint32_t				obj_id;
} __packed;
STATIC_ASSERT(FIELD_SIZEOF(verobj_freelist_rw, obj_id) == IDX_SIZE,
	"type of obj_id must be consistent with IDX_SIZE");
STATIC_ASSERT(FIELD_SIZEOF(verobj_freelist_rw_ret, obj_id) == IDX_SIZE,
	"type of obj_id must be consistent with IDX_SIZE");
boundary1(freelist, verobj_freelist_rw, obj_id)
boundary1(freelist_ret, verobj_freelist_rw_ret, obj_id)

// metadata field
struct verobj_metadata_rw {
	struct legomem_rw_fpgamsg		unused;
	struct obj_array_metadata		op;
} __packed;
struct verobj_metadata_rw_ret {
	struct legomem_rw_fpgamsg_resp		unused;
	struct obj_array_metadata		op;
} __packed;
struct carryon_objid {
	struct verobj_metadata_rw		unused;
	uint32_t				obj_id;
} __packed;
boundary2(metadata, verobj_metadata_rw, op, obj_array_metadata, vld)
boundary2(metadata, verobj_metadata_rw, op, obj_array_metadata, flags)
boundary2(metadata, verobj_metadata_rw, op, obj_array_metadata, pid)
boundary2(metadata, verobj_metadata_rw, op, obj_array_metadata, size)
boundary2(metadata, verobj_metadata_rw, op, obj_array_metadata, unused)
boundary2(metadata, verobj_metadata_rw, op, obj_array_metadata, objptr)
boundary1(carryon, carryon_objid, obj_id)
boundary2(metadata_ret, verobj_metadata_rw_ret, op, obj_array_metadata, vld)
boundary2(metadata_ret, verobj_metadata_rw_ret, op, obj_array_metadata, flags)
boundary2(metadata_ret, verobj_metadata_rw_ret, op, obj_array_metadata, pid)
boundary2(metadata_ret, verobj_metadata_rw_ret, op, obj_array_metadata, size)
boundary2(metadata_ret, verobj_metadata_rw_ret, op, obj_array_metadata, unused)
boundary2(metadata_ret, verobj_metadata_rw_ret, op, obj_array_metadata, objptr)

#endif /* _LEGO_MEM_MULTI_VER_OBJ_H_ */
