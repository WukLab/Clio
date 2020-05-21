/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "multi_ver_obj.h"
#include "multi_ver_obj_ops.h"
#include "multi_ver_obj_bramdata.h"

using namespace hls;

/* TODO: handle failure */
void waitqueue1(stream<struct wait_if> &from_parser, stream<struct wait_if> &towq2,
		stream<struct data_if> &tosq_net,
		stream<struct data_record_if> &tosq_soc, stream<struct data_record_if> &tosq_mem,
		stream<struct data_if> &from_disp, stream<struct version_bram_if> &from_ver)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum wq1_fsm {INIT, READY, DATA};

	// stateful variable, remain the same between cycles
	static wq1_fsm state						= INIT;
	static struct verobj_data_copy ep_data_wqcopy			= {0,0,0};
	//static ap_uint<1> drop 					= 0;	// drop if error occurs

	// delay structure
	static struct delay<struct wait_if> towq2_delay			= {0,0};
	static struct delay<struct data_if> tosq_net_delay		= {0,0};
	static struct delay<struct data_record_if> tosq_soc_delay	= {0,0};
	static struct delay<struct data_record_if> tosq_mem_delay	= {0,0};

	// temporary variable
	struct wait_if wq1_pkt						= {0,0,0};
	struct wait_if wq2_pkt						= {0,0,0};
	struct data_if net_pkt						= {0,0};
	struct data_record_if soc_pkt					= {0,0};
	struct data_record_if mem_pkt					= {0,0};
	struct data_if disp_pkt						= {0,0};
	struct version_bram_if version					= {0,0,0};
	unsigned long addr						= 0;

	/* release delayed packet */
	release_delay_pkt(towq2, towq2_delay);
	release_delay_pkt(tosq_net, tosq_net_delay);
	release_delay_pkt(tosq_soc, tosq_soc_delay);
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
		if (from_disp.empty() || from_parser.empty())
			break;
		wq1_pkt = from_parser.read();
		disp_pkt = from_disp.read();

		switch (wq1_pkt.type) {
		case OBJ_CREATE:
			assert(wq1_pkt.last == 1);

			wq2_pkt.pkt = wq1_pkt.pkt;
			wq2_pkt.last = wq1_pkt.last;
			if (field(disp_pkt.pkt, hdr_opcode) == OP_REQ_ALLOC_RESP) {
				field(wq2_pkt.pkt, metadata_objptr) = field(disp_pkt.pkt, mallocret_addr);
			} else {
				// if it's first round id allocation,
				// obj_id has already been allocated by parser
				if (field(wq1_pkt.pkt, carryon_obj_id) == 0xFFFFFFFF) {
					field(wq2_pkt.pkt, carryon_obj_id)
						= field(disp_pkt.pkt, freelist_ret_obj_id);
					field(wq2_pkt.pkt, mem_va)
						= ep_data_wqcopy.objarray_ptr
						+ ((unsigned long)field(disp_pkt.pkt, freelist_ret_obj_id)
						<< LOG_OBJ_METADATA_SIZE);
				}
			}
			delay_pkt(towq2_delay, wq2_pkt);
			break;

		case OBJ_DELETE:
			// invalidate metadata and record
			invalidate_metadata_req(mem_pkt, wq1_pkt.pkt, tosq_mem_delay, ep_data_wqcopy, DROP);

			// free version array address
			soc_free_req(soc_pkt, wq1_pkt.pkt, tosq_soc_delay,
				    field(disp_pkt.pkt, metadata_ret_objptr),
				    field(disp_pkt.pkt, metadata_ret_size), DROP, ep_data_wqcopy);

			// reply
			send_resp_hdronly(net_pkt, wq1_pkt.pkt, tosq_net_delay,
					  OP_REQ_VEROBJ_DELETE_RESP,
					  sizeof(struct verobj_create_delete_ret), 0);
			break;

		case OBJ_READ:
			assert(wq1_pkt.last == 1);

			// version field in user request being 0 means,
			// means user didn't provide version
			if (field(wq1_pkt.pkt, verobjrw_version) == 0) {
				assert(!from_ver.empty());
				version = from_ver.read();
				addr = (unsigned long)field(disp_pkt.pkt, metadata_ret_objptr)
				     + ((unsigned long)field(disp_pkt.pkt, metadata_ret_size)
				     * (unsigned long)version.version);
			} else {
				addr = (unsigned long)field(disp_pkt.pkt, metadata_ret_objptr)
				     + ((unsigned long)field(disp_pkt.pkt, metadata_ret_size)
				     * (unsigned long)field(wq1_pkt.pkt, verobjrw_version));
			}

			verobj2mem_rw_req(mem_pkt, wq1_pkt.pkt, tosq_mem_delay, addr,
				field(disp_pkt.pkt, metadata_ret_size), OP_REQ_READ, 1, SQ, ep_data_wqcopy);
			break;

		case OBJ_WRITE:
			if (wq1_pkt.last == 0)
				state = DATA;

			assert(!from_ver.empty());
			version = from_ver.read();
			addr = (unsigned long)field(disp_pkt.pkt, metadata_ret_objptr)
			     + ((unsigned long)field(disp_pkt.pkt, metadata_ret_size)
			     * (unsigned long)version.version);
			verobj2mem_rw_req(mem_pkt, wq1_pkt.pkt, tosq_mem_delay, addr,
				field(disp_pkt.pkt, metadata_ret_size), OP_REQ_WRITE, wq1_pkt.last, SQ, ep_data_wqcopy);
			break;

		default:
			break;
		}
		break;

	case DATA:
		if (from_parser.empty())
			break;
		wq1_pkt = from_parser.read();

		mem_pkt.pkt = wq1_pkt.pkt;
		mem_pkt.last = wq1_pkt.last;
		if (wq1_pkt.last == 1)
			state = READY;
		delay_pkt(tosq_mem_delay, mem_pkt);
		break;

	default:
		break;
	}
}
