/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "multi_ver_obj.h"
#include "multi_ver_obj_bramdata.h"

#define after(unknown, known)	((int16_t)(known) - (int16_t)(unknown) < 0)

using namespace hls;

void dispatcher(stream<struct data_if> &data_in,
		stream<struct ctrl_if> &ctrl_in, stream<struct ctrl_if> &ctrl_out,
		stream<struct record_out_if> &soc_records, stream<struct record_out_if> &mem_records,
		stream<struct data_if> &to_parser, stream<struct data_if> &to_wq1,
		stream<struct data_if> &to_wq2, stream<struct data_if> &to_sq)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum fsm {
		// init phase
		PID_ALLOC, PID_ALLOC_WAIT, OBJ_ALLOC, OBJ_ALLOC_WAIT, FLIST_ALLOC, FLIST_ALLOC_WAIT,
		// running phase
		HEADER, DATA
	};

	/* stateful variables */
	static fsm state					= PID_ALLOC;
	static component to_where				= DROP;
	static unsigned int CREATE_mem_seqid			= 0;
	static unsigned int CREATE_soc_seqid			= 0;

	// reply holder
	static struct verobj_data_copy	verobj_data		= {0,0,0};

	// delay structure
	static struct delay<struct data_if> to_parser_delay	= {0,0};
	static struct delay<struct data_if> to_wq1_delay	= {0,0};
	static struct delay<struct data_if> to_wq2_delay	= {0,0};
	static struct delay<struct data_if> to_sq_delay		= {0,0};
	static struct delay<struct ctrl_if> ctrlout_delay	= {0,0};

	/* temporary variables */
	struct data_if in_pkt					= {0,0};
	struct data_if out_pkt					= {0,0};
	struct ctrl_if ctrl_pkt					= {0,0};
	struct record_out_if record				= {0,0,0};

	/* release delayed packet */
	release_delay_pkt(ctrl_out, ctrlout_delay);
	release_delay_pkt(to_parser, to_parser_delay);
	release_delay_pkt(to_wq1, to_wq1_delay);
	release_delay_pkt(to_wq2, to_wq2_delay);
	release_delay_pkt(to_sq, to_sq_delay);

	switch (state) {
	case HEADER:
		if (data_in.empty())
			break;
		in_pkt = data_in.read();

		switch (field(in_pkt.pkt, hdr_opcode)) {
		case OP_REQ_VEROBJ_CREATE:
		case OP_REQ_VEROBJ_DELETE:
		case OP_REQ_VEROBJ_READ:
			out_pkt = in_pkt;
			delay_pkt(to_parser_delay, out_pkt);
			break;

		case OP_REQ_VEROBJ_WRITE:
			out_pkt = in_pkt;
			if (out_pkt.last == 0) {
				state = DATA;
				to_where = PARSER;
			}
			delay_pkt(to_parser_delay, out_pkt);
			break;

		case OP_REQ_ALLOC_RESP:
			out_pkt = in_pkt;
			assert(!soc_records.empty());
			record = soc_records.read();
			CREATE_soc_seqid++;
			if (after(CREATE_soc_seqid, CREATE_mem_seqid)) {
				/* waiting request is in waitqueue1 */
				delay_pkt(to_wq1_delay, out_pkt);
			} else {
				/* waiting request is in waitqueue2 */
				delay_pkt(to_wq2_delay, out_pkt);
			}
			break;

		case OP_REQ_FREE_RESP:
			assert(!soc_records.empty());
			soc_records.read();
			break;

		case OP_REQ_READ_RESP:
			out_pkt = in_pkt;
			assert(!mem_records.empty());
			record = mem_records.read();
			if (record.opcode == OP_REQ_VEROBJ_CREATE) {
				CREATE_mem_seqid++;
				if (after(CREATE_mem_seqid, CREATE_soc_seqid)) {
					/* waiting request is in waitqueue1 */
					delay_pkt(to_wq1_delay, out_pkt);
				} else {
					/* waiting request is in waitqueue2 */
					delay_pkt(to_wq2_delay, out_pkt);
				}
			} else {
				switch (record.dest_comp) {
				case WQ1:
					delay_pkt(to_wq1_delay, out_pkt);
					break;
				case SQ:
					field(out_pkt.pkt, hdr_pid) = record.usr_pid;
					field(out_pkt.pkt, hdr_opcode) = OP_REQ_VEROBJ_READ_RESP;
					field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_NET;
					field(out_pkt.pkt, hdr_req_status) = 0;

					if (out_pkt.last == 0) {
						state = DATA;
						to_where = SQ;
					}
					delay_pkt(to_sq_delay, out_pkt);
					break;
				case DROP:
					break;
				case WQ2:
				case PARSER:
				default:
					assert(false);
					break;
				}
			}
			break;

		case OP_REQ_WRITE_RESP:
			out_pkt = in_pkt;
			assert(!mem_records.empty());
			record = mem_records.read();
			switch (record.dest_comp) {
			case WQ1:
				delay_pkt(to_wq1_delay, out_pkt);
				break;
			case SQ:
				field(out_pkt.pkt, hdr_pid) = record.usr_pid;
				field(out_pkt.pkt, hdr_opcode) = OP_REQ_VEROBJ_WRITE_RESP;
				field(out_pkt.pkt, hdr_size) = sizeof(struct verobj_read_write_ret);
				field(out_pkt.pkt, hdr_cont) = LEGOMEM_CONT_NET;
				field(out_pkt.pkt, hdr_req_status) = 0;
				delay_pkt(to_sq_delay, out_pkt);
				break;
			case DROP:
				break;
			case WQ2:
			case PARSER:
			default:
				assert(false);
				break;
			}
			break;

		default:
			assert(false);
			break;
		}
		break;

	case DATA:
		if (data_in.empty())
			break;
		in_pkt = data_in.read();
		out_pkt = in_pkt;
		if (out_pkt.last == 1)
			state = HEADER;

		switch (to_where) {
		case PARSER:
			delay_pkt(to_parser_delay, out_pkt);
			break;
		case SQ:
			delay_pkt(to_sq_delay, out_pkt);
			break;
		default:
			assert(false);
			break;
		}
		break;


	/* below are initialization path */
	case PID_ALLOC:
		ctrl_pkt.pkt.cmd = CMD_LEGOMEM_CTRL_CREATE_PROC;
		ctrl_pkt.pkt.epid = SOC_XBAR_EPID;
		ctrl_pkt.pkt.addr = SOC_XBAR_ADDR;
		delay_pkt(ctrlout_delay, ctrl_pkt);

		state = PID_ALLOC_WAIT;
		break;

	case PID_ALLOC_WAIT:
		if (ctrl_in.empty())
			break;
		ctrl_pkt = ctrl_in.read();

		if (ctrl_pkt.pkt.cmd != CMD_LEGOMEM_CTRL_CREATE_PROC  ||
		    ctrl_pkt.pkt.param32 == 0) {
			state = PID_ALLOC;			// retry
		} else {
			verobj_data.pid = ctrl_pkt.pkt.param32;
			state = OBJ_ALLOC;
		}
		break;

	case OBJ_ALLOC:
		ctrl_pkt.pkt.cmd = CMD_LEGOMEM_CTRL_ALLOC;
		ctrl_pkt.pkt.epid = SOC_XBAR_EPID;
		ctrl_pkt.pkt.addr = SOC_XBAR_ADDR;
		ctrl_pkt.pkt.param32 = OBJ_ARRAY_SIZE;
		ctrl_pkt.pkt.param8 = verobj_data.pid;
		delay_pkt(ctrlout_delay, ctrl_pkt);

		state = OBJ_ALLOC_WAIT;
		break;

	case OBJ_ALLOC_WAIT:
		if (ctrl_in.empty())
			break;
		ctrl_pkt = ctrl_in.read();

		if (ctrl_pkt.pkt.cmd != CMD_LEGOMEM_CTRL_ALLOC  ||
		    ctrl_pkt.pkt.param32 == 0) {
			state = OBJ_ALLOC;			// retry
		} else {
			verobj_data.objarray_ptr = ctrl_pkt.pkt.param32;
			state = FLIST_ALLOC;
		}

		break;

	case FLIST_ALLOC:
		ctrl_pkt.pkt.cmd = CMD_LEGOMEM_CTRL_ALLOC;
		ctrl_pkt.pkt.epid = SOC_XBAR_EPID;
		ctrl_pkt.pkt.addr = SOC_XBAR_ADDR;
		ctrl_pkt.pkt.param32 = FREELIST_SIZE;
		ctrl_pkt.pkt.param8 = verobj_data.pid;
		delay_pkt(ctrlout_delay, ctrl_pkt);

		state = FLIST_ALLOC_WAIT;
		break;

	case FLIST_ALLOC_WAIT:
		if (ctrl_in.empty())
			break;
		ctrl_pkt = ctrl_in.read();

		if (ctrl_pkt.pkt.cmd != CMD_LEGOMEM_CTRL_ALLOC  ||
		    ctrl_pkt.pkt.param32 == 0) {
			state = FLIST_ALLOC;			// retry
		} else {
			verobj_data.freelist_ptr = ctrl_pkt.pkt.param32;
			state = HEADER;
		}

		// send information to other sub component
		field(out_pkt.pkt, verobj_data_pid) = verobj_data.pid;
		field(out_pkt.pkt, verobj_data_objarray_ptr) = verobj_data.objarray_ptr;
		field(out_pkt.pkt, verobj_data_freelist_ptr) = verobj_data.freelist_ptr;
		out_pkt.last = 1;
		delay_pkt(to_parser_delay, out_pkt);
		delay_pkt(to_wq1_delay, out_pkt);
		delay_pkt(to_wq2_delay, out_pkt);
		break;

	default:
		break;
	}
}
