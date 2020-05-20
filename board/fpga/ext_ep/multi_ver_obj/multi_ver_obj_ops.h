/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_MULTI_VER_OBJ_OPS_H_
#define _LEGO_MEM_MULTI_VER_OBJ_OPS_H_

#include "multi_ver_obj.h"

// send pending request to waitqueue
#define send_req_towq(out_pkt, in_pkt, delay_fifo, req_type, is_last)			\
do {											\
	out_pkt.pkt = in_pkt;								\
	out_pkt.type = req_type;							\
	out_pkt.last = is_last;								\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// read metadata request
#define read_metadata_req(out_pkt, in_pkt, delay_fifo, _ep_data, _dest_comp)		\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_pid) = _ep_data.pid;					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_READ;					\
	field(out_pkt.pkt, hdr_size) = sizeof(struct legomem_rw_fpgamsg);		\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_MEM;				\
	field(out_pkt.pkt, mem_va) = _ep_data.objarray_ptr +				\
		((unsigned long)field(in_pkt, verobjrw_obj_id) 				\
		<< LOG_OBJ_METADATA_SIZE);						\
	field(out_pkt.pkt, mem_size) = OBJ_METADATA_SIZE;				\
	out_pkt.last = 1;								\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_MEM;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// write metadata request, header only
#define write_metadata_req_hdronly(out_pkt, in_pkt, delay_fifo)		\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_WRITE;					\
	field(out_pkt.pkt, hdr_size) = 							\
		sizeof(struct legomem_rw_fpgamsg) + OBJ_METADATA_SIZE;			\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_MEM;				\
	out_pkt.last = 1;								\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// invalidate metadata request
#define invalidate_metadata_req(out_pkt, in_pkt, delay_fifo, _ep_data, _dest_comp)	\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_pid) = _ep_data.pid;					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_WRITE;					\
	field(out_pkt.pkt, hdr_size) = 							\
		sizeof(struct legomem_rw_fpgamsg) + OBJ_METADATA_SIZE;			\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_MEM;				\
	field(out_pkt.pkt, mem_va) = _ep_data.objarray_ptr +				\
		field(in_pkt, verobjcd_obj_size_id) << LOG_OBJ_METADATA_SIZE;		\
	field(out_pkt.pkt, mem_size) = OBJ_METADATA_SIZE;				\
	field(out_pkt.pkt, metadata_vld) = 0;						\
	field(out_pkt.pkt, metadata_flags) = 0;						\
	field(out_pkt.pkt, metadata_pid) = 0;						\
	field(out_pkt.pkt, metadata_size) = 0;						\
	field(out_pkt.pkt, metadata_unused) = 0;					\
	field(out_pkt.pkt, metadata_objptr) = 0;					\
	out_pkt.last = 1;								\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_MEM;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// SoC alloc/free request
#define soc_alloc_req(out_pkt, in_pkt, delay_fifo, _dest_comp)				\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_ALLOC;					\
	field(out_pkt.pkt, hdr_size) = sizeof(struct legomem_alloc_free_fpgamsg);	\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_SOC;				\
	field(out_pkt.pkt, malloc_len) = 						\
		field(in_pkt, verobjcd_obj_size_id) << LOG_VERSION_ARRAY_COUNT;		\
	field(out_pkt.pkt, malloc_vregion_idx) = field(in_pkt, verobjcd_vregion_idx);	\
	field(out_pkt.pkt, malloc_vm_flags) = field(in_pkt, verobjcd_vm_flags);		\
	out_pkt.last = 1;								\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_SOC;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

#define soc_free_req(out_pkt, in_pkt, delay_fifo, addr, size, _dest_comp)		\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_FREE;					\
	field(out_pkt.pkt, hdr_size) = sizeof(struct legomem_alloc_free_fpgamsg);	\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_SOC;				\
	field(out_pkt.pkt, malloc_addr) = (addr);					\
	field(out_pkt.pkt, malloc_len) = (uint64_t)(size) << LOG_VERSION_ARRAY_COUNT;	\
	out_pkt.last = 1;								\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_SOC;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// freelist dequeue request
/*
 * dequeue req without advance pointer is used to send dummy request to core_mem
 * in order to maintain pipeline dependency without extra headache
 * useful for first round of object ID allocation
 */
#define freelist_dequeue_req_wo_advance(out_pkt, in_pkt, 				\
					delay_fifo, _ep_data, _dest_comp)		\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_pid) = _ep_data.pid;					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_READ;					\
	field(out_pkt.pkt, hdr_size) = sizeof(struct legomem_rw_fpgamsg);		\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_MEM;				\
	field(out_pkt.pkt, mem_va) = _ep_data.frontptr();				\
	field(out_pkt.pkt, mem_size) = IDX_SIZE;					\
	out_pkt.last = 1;								\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_MEM;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

#define freelist_dequeue_req(out_pkt, in_pkt, delay_fifo, _ep_data, _to_comp)		\
do {											\
	freelist_dequeue_req_wo_advance(out_pkt, 					\
		in_pkt, delay_fifo, _ep_data, _to_comp);				\
	_ep_data.head_advance();							\
} while(0)

// freelist enqueue request
#define freelist_enqueue_req(out_pkt, in_pkt, delay_fifo, _ep_data, _dest_comp)		\
do {											\
	_ep_data.tail_advance();							\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_pid) = _ep_data.pid;					\
	field(out_pkt.pkt, hdr_opcode) = OP_REQ_WRITE;					\
	field(out_pkt.pkt, hdr_size) = sizeof(struct legomem_rw_fpgamsg) + IDX_SIZE;	\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_MEM;				\
	field(out_pkt.pkt, mem_va) = _ep_data.tailptr();				\
	field(out_pkt.pkt, mem_size) = IDX_SIZE;					\
	field(out_pkt.pkt, freelist_obj_id) = field(in_pkt, verobjcd_obj_size_id);	\
	out_pkt.last = 1;								\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_MEM;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// convert multi_ver_obj request to mem r/w request
#define verobj2mem_rw_req(out_pkt, in_pkt, delay_fifo, addr, 				\
				size, _opcode, is_last, _dest_comp)			\
do {											\
	out_pkt.pkt = in_pkt;								\
	field(out_pkt.pkt, hdr_opcode) = (_opcode);					\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_MEM;				\
	field(out_pkt.pkt, mem_va) = (addr);						\
	field(out_pkt.pkt, mem_size) = (size);						\
	out_pkt.last = (is_last);							\
	out_pkt.opcode = field(in_pkt, hdr_opcode);					\
	out_pkt.endpoint = EP_MEM;							\
	out_pkt.dest_comp = _dest_comp;							\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

// send response with header only
// prepare other field first then use this macro
#define send_resp_hdronly(out_pkt, in_pkt, delay_fifo, req_type, size, error_code)	\
do {											\
	field(out_pkt.pkt, hdr) = field(in_pkt, hdr);					\
	field(out_pkt.pkt, hdr_opcode) = (req_type);					\
	field(out_pkt.pkt, hdr_size) = (size);						\
	field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_NET;				\
	field(out_pkt.pkt, hdr_req_status) = (error_code);				\
	out_pkt.last = 1;								\
	delay_pkt(delay_fifo, out_pkt);							\
} while(0)

#endif /* _LEGO_MEM_MULTI_VER_OBJ_OPS_H_ */
