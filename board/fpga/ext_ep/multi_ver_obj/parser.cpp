/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <iostream>
#include "multi_ver_obj.h"
#include "multi_ver_obj_ops.h"
#include "multi_ver_obj_bramdata.h"

//#define PARSER_PRINT

using namespace hls;

/* TODO: share flags */
void parser(stream<struct data_if> &data_in, stream<struct version_bram_if> &to_bram,
	    stream<struct data_record_if> &tosq1, stream<struct data_record_if> &tosq2,
	    stream<struct wait_if> &towq)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum parser_fsm {INIT, READY, DATATOWAITQUEUE};

	// stateful variable, remain the same between cycles
	static parser_fsm state	= INIT;
	static multi_ver_obj_data ep_data;

	// delay structure
	static struct delay<struct version_bram_if> to_bram_delay	= {0,0};
	static struct delay<struct data_record_if> tosq1_delay		= {0,0};
	static struct delay<struct data_record_if> tosq2_delay		= {0,0};
	static struct delay<struct wait_if> towq_delay			= {0,0};

	// temporary variable
	struct data_if indata_pkt                    			= {0,0};
	struct data_record_if sq1_pkt					= {0,0,0,0,0};
	struct data_record_if sq2_pkt					= {0,0,0,0,0};
	struct wait_if wq_pkt						= {0,0,0};
	struct version_bram_if to_bram_pkt				= {0,0,0};

	/* release delayed packet */
	release_delay_pkt(to_bram, to_bram_delay);
	release_delay_pkt(tosq1, tosq1_delay);
	release_delay_pkt(tosq2, tosq2_delay);
	release_delay_pkt(towq, towq_delay);

	switch (state) {
	case INIT:
		if (data_in.empty())
			break;
		indata_pkt = data_in.read();

		ep_data.pid = field(indata_pkt.pkt, verobj_data_pid);
		ep_data.objarray_ptr = field(indata_pkt.pkt, verobj_data_objarray_ptr);
		ep_data.freelist_ptr = field(indata_pkt.pkt, verobj_data_freelist_ptr);
		state = READY;

#ifndef PARSER_PRINT
		std::cout << "Endpoint PID: " << ep_data.pid
			  << "\nEndpoint object array addr: " << ep_data.objarray_ptr
			  << "\nEndpoint object id freelist addr: " << ep_data.freelist_ptr << std::endl;
#endif
		break;

	case READY:
		if (data_in.empty())
			break;
		indata_pkt = data_in.read();

		switch (field(indata_pkt.pkt, hdr_opcode)) {
		// data from network
		case OP_REQ_VEROBJ_CREATE:
			if (!ep_data.empty()) {
				// allocate version array address
				soc_alloc_req(sq1_pkt, indata_pkt.pkt, tosq1_delay, WQ1);

				/*
				 * if obj allocation in the first round, send memory request
				 * maintain pipeline dependency
				 */
				if (ep_data.is_firstround())
					freelist_dequeue_req_wo_advance(sq2_pkt,
						indata_pkt.pkt, tosq2_delay, ep_data, WQ1);
				else
					freelist_dequeue_req(sq2_pkt,
						indata_pkt.pkt, tosq2_delay, ep_data, WQ1);

				/*
				 * prepare write request and send it to waitqueue, waiting for:
				 * 1. obj_id (if not first round)
				 * 2. version array objptr
				 */
				field(wq_pkt.pkt, mem_size) = OBJ_METADATA_SIZE;
				field(wq_pkt.pkt, metadata_vld) = 1;
				field(wq_pkt.pkt, metadata_pid) = field(indata_pkt.pkt, hdr_pid);
				field(wq_pkt.pkt, metadata_size) = field(indata_pkt.pkt, verobjcd_obj_size_id);
				/* TODO: vm_flags private and share */
				if (field(indata_pkt.pkt, verobjcd_vm_flags) == 0)
					field(wq_pkt.pkt, metadata_flags) = VEROBJ_PRIVATE;
				else
					field(wq_pkt.pkt, metadata_flags) = VEROBJ_SHARE;
				/* if not firstround, mark it for waitqueue reference */
				if (ep_data.is_firstround()) {
					ep_data.head_advance_firstround();
					field(wq_pkt.pkt, carryon_obj_id) = ep_data.firstround_cnt;
					field(wq_pkt.pkt, mem_va)
						= ep_data.objarray_ptr
						+ ((unsigned long)field(wq_pkt.pkt, carryon_obj_id)
						<< LOG_OBJ_METADATA_SIZE);
				} else {
					field(wq_pkt.pkt, carryon_obj_id) = 0xFFFFFFFF;
				}

				/* keep user pid */
				write_metadata_req_hdronly(wq_pkt, indata_pkt.pkt, towq_delay);
			} else {
				// no available object ID, return with error code
				// TODO: out-of-space error code ?
				send_resp_hdronly(sq1_pkt, indata_pkt.pkt, tosq1_delay,
						  OP_REQ_VEROBJ_CREATE_RESP,
						  sizeof(struct verobj_create_delete_ret), 0xF);
			}
			break;

		case OP_REQ_VEROBJ_DELETE:
			// read version array address
			read_metadata_req(sq1_pkt, indata_pkt.pkt, tosq1_delay, ep_data, WQ1);

			// write object ID to free list
			freelist_enqueue_req(sq2_pkt, indata_pkt.pkt, tosq2_delay, ep_data, DROP);

			// leave the request as it is
			send_req_towq(wq_pkt, indata_pkt.pkt, towq_delay, OBJ_DELETE, 1);
			break;

		case OP_REQ_VEROBJ_READ:
			// read version array address
			read_metadata_req(sq1_pkt, indata_pkt.pkt, tosq1_delay, ep_data, WQ1);

			// request version number
			// version field in user request being 0 means,
			// means user didn't provide version
			if (field(indata_pkt.pkt, verobjrw_version) == 0) {
				to_bram_pkt.rw = VERSION_READ;
				to_bram_pkt.obj_id = field(indata_pkt.pkt, verobjrw_obj_id);
				to_bram_pkt.version = 0xFFFF;				// sanity check
				delay_pkt(to_bram_delay, to_bram_pkt);
			}

			// leave the request as it is
			send_req_towq(wq_pkt, indata_pkt.pkt, towq_delay, OBJ_READ, 1);
			break;

		case OP_REQ_VEROBJ_WRITE:
			// read version array address
			read_metadata_req(sq1_pkt, indata_pkt.pkt, tosq1_delay, ep_data, WQ1);

			// request version number
			to_bram_pkt.rw = VERSION_INC;
			to_bram_pkt.obj_id = field(indata_pkt.pkt, verobjrw_obj_id);
			to_bram_pkt.version = 0xFFFF;					// sanity check
			delay_pkt(to_bram_delay, to_bram_pkt);

			// write data possibly span multiple cycles
			// leave the request as it is
			send_req_towq(wq_pkt, indata_pkt.pkt, towq_delay, OBJ_WRITE, indata_pkt.last);
			if (indata_pkt.last == 0)
				state = DATATOWAITQUEUE;
			break;

		default:
			break;
		}
		break;

	case DATATOWAITQUEUE:
		if (data_in.empty())
			break;
		indata_pkt = data_in.read();

		wq_pkt.pkt = indata_pkt.pkt;
		send_req_towq(wq_pkt, indata_pkt.pkt, towq_delay, OBJ_WRITE, indata_pkt.last);
		if (indata_pkt.last == 1)
			state = READY;
		break;

	default:
		break;
	}
}
