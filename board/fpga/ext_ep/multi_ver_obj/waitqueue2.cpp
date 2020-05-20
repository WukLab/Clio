/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#include "multi_ver_obj.h"
#include "multi_ver_obj_ops.h"
#include "multi_ver_obj_bramdata.h"

using namespace hls;

/* TODO: handle failure */
void waitqueue2(stream<struct wait_if> &from_wq1, stream<struct data_if> &from_disp,
		stream<struct data_if> &tosq_net, stream<struct data_record_if> &tosq_mem)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum wq1_fsm {INIT, READY};

	// stateful variable, remain the same between cycles
	static wq1_fsm state						= INIT;
	static struct verobj_data_copy ep_data_wqcopy			= {0,0,0};
	//static ap_uint<1> drop 					= 0;		// drop if error occurs

	// delay structure
	static struct delay<struct data_if> tosq_net_delay		= {0,0};
	static struct delay<struct data_record_if> tosq_mem_delay	= {0,0};

	// temporary variable
	struct wait_if wq_pkt						= {0,0,0};
	struct data_if net_pkt						= {0,0};
	struct data_record_if mem_pkt					= {0,0};
	struct data_if disp_pkt						= {0,0};

	/* release delayed packet */
	release_delay_pkt(tosq_net, tosq_net_delay);
	release_delay_pkt(tosq_mem, tosq_mem_delay);

	switch (state) {
	case INIT:
		if (from_disp.empty())
			break;
		disp_pkt = from_disp.read();

		ep_data_wqcopy.pid = field(disp_pkt.pkt, verobj_data_pid);
		ep_data_wqcopy.objarray_ptr = field(disp_pkt.pkt, verobj_data_objarray_ptr);
		ep_data_wqcopy.freelist_ptr = field(disp_pkt.pkt, verobj_data_freelist_ptr);
		state = READY;
		break;

	case READY:
		if (from_disp.empty())
			break;
		assert(!from_wq1.empty());
		wq_pkt = from_wq1.read();
		disp_pkt = from_disp.read();

		switch (wq_pkt.type) {
		case OBJ_CREATE:
			assert(wq_pkt.last == 1 && disp_pkt.last == 1);

			// write metadata
			mem_pkt.pkt = wq_pkt.pkt;
			mem_pkt.last = wq_pkt.last;
			field(mem_pkt.pkt, hdr_pid) = ep_data_wqcopy.pid;
			if (field(disp_pkt.pkt, hdr_opcode) == OP_REQ_ALLOC_RESP) {
				field(mem_pkt.pkt, metadata_objptr) = field(disp_pkt.pkt, mallocret_addr);
			} else {
				// if it's first round id allocation,
				// obj_id has already been allocated by parser
				if (field(wq_pkt.pkt, carryon_obj_id) == 0xFFFFFFFF) {
					field(mem_pkt.pkt, carryon_obj_id)
						= field(disp_pkt.pkt, freelist_ret_obj_id);
					field(mem_pkt.pkt, mem_va)
						= ep_data_wqcopy.objarray_ptr
						+ ((unsigned long)field(disp_pkt.pkt, freelist_ret_obj_id)
						<< LOG_OBJ_METADATA_SIZE);
				}
			}
			mem_pkt.opcode = OP_REQ_VEROBJ_CREATE;
			mem_pkt.endpoint = EP_MEM;
			mem_pkt.dest_comp = DROP;
			delay_pkt(tosq_mem_delay, mem_pkt);

			// reply
			field(net_pkt.pkt, verobjcdret_obj_id) = field(mem_pkt.pkt, carryon_obj_id);
			send_resp_hdronly(net_pkt, wq_pkt.pkt, tosq_net_delay,
					  OP_REQ_VEROBJ_CREATE_RESP,
					  sizeof(struct verobj_create_delete_ret), 0);
			break;

		default:
			assert(false);
			break;
		}
		break;

	default:
		break;
	}
}
