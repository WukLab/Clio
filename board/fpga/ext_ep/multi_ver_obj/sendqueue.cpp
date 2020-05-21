/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "multi_ver_obj.h"

//#define SENDQUEUE_PRINT

// record pending request information, send it to dispatcher
#define record_req(in_pkt, recorder)							\
do {											\
	struct record_out_if record_pkt = {0,0,0};					\
	record_pkt.opcode = in_pkt.opcode;						\
	record_pkt.endpoint = in_pkt.endpoint;						\
	record_pkt.dest_comp = in_pkt.dest_comp;					\
	record_pkt.usr_pid = in_pkt.usr_pid;						\
	recorder.write(record_pkt);							\
} while(0)

using namespace hls;

/*
 * We want to arbitrate among parser, wq1, wq2, and dispatcher rather than each port
 * to guarantee dependency correctness.
 * We don't want to check all ports to perform arbitration,
 * based on behavior of parser, wq1, and wq2, we can make following assumption:
 * check "from_parser1" to see if parser has data available
 * check "from_wq1_mem" to see if waitqueue1 has data available
 * check "from_wq2_mem" to see if waitqueue2 has data available
 * check "from_dispatch" to see if dispatcher has data available
 */
void sendqueue(stream<struct data_record_if> &from_parser1, stream<struct data_record_if> &from_parser2,
	       stream<struct data_if> &from_wq1_net, stream<struct data_record_if> &from_wq1_soc,
	       stream<struct data_record_if> &from_wq1_mem, stream<struct data_if> &from_wq2_net,
	       stream<struct data_record_if> &from_wq2_mem, stream<struct data_if> &from_dispatch,
	       stream<struct record_out_if> &soc_wip, stream<struct record_out_if> &mem_wip,
	       stream<struct data_if> &data_out)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum sendqueue_fsm {READY, PARSER2, WQ1_MEM_DATA, WQ1_SOC, WQ1_NET, WQ2_NET, DISPATCHER_DATA};
	enum Source {NONE, PARSER, WQ1, WQ2, DISPATCHER};

	static sendqueue_fsm state			= READY;
	static ap_uint<3> port_mask			= 0;

	Source decision					= NONE;
	struct data_record_if data_record_pkt		= {0,0};
	struct data_if data_pkt				= {0,0};
	struct data_if out_pkt				= {0,0};
	struct record_out_if record			= {0,0,0};

	switch (state) {
	case READY:
		if (!from_dispatch.empty())
			decision = DISPATCHER;
		else if (!from_wq2_mem.empty())
			decision = WQ2;
		else if (!from_wq1_mem.empty())
			decision = WQ1;
		else if (!from_parser1.empty())
			decision = PARSER;
		else
			decision = NONE;

		assert(port_mask == 0);
		switch (decision) {
		case PARSER:
			data_record_pkt = from_parser1.read();
			assert(data_record_pkt.last == 1);

			if (!from_parser2.empty())
				state = PARSER2;

			switch (data_record_pkt.endpoint) {
			case EP_SOC:
				record_req(data_record_pkt, soc_wip);
				break;
			case EP_MEM:
				record_req(data_record_pkt, mem_wip);
				break;
			default:
				assert(false);
			}
			out_pkt.pkt = data_record_pkt.pkt;
			out_pkt.last = data_record_pkt.last;
			data_out.write(out_pkt);
			break;

		case WQ1:
			data_record_pkt = from_wq1_mem.read();

			if (data_record_pkt.last == 0)
				port_mask[0] = 1;
			if (!from_wq1_soc.empty())
				port_mask[1] = 1;
			if (!from_wq1_net.empty())
				port_mask[2] = 1;

			if (port_mask[0] == 1)
				state = WQ1_MEM_DATA;
			else if (port_mask[1] == 1)
				state = WQ1_SOC;
			else if (port_mask[2] == 1)
				state = WQ1_NET;
			decision = WQ2;

			switch (data_record_pkt.endpoint) {
			case EP_MEM:
				record_req(data_record_pkt, mem_wip);
				break;
			default:
				assert(false);
			}
			out_pkt.pkt = data_record_pkt.pkt;
			out_pkt.last = data_record_pkt.last;
			data_out.write(out_pkt);
			break;

		case WQ2:
			data_record_pkt = from_wq2_mem.read();

			assert(data_record_pkt.last == 1);
			if (!from_wq2_net.empty())
				state = WQ2_NET;

			switch (data_record_pkt.endpoint) {
			case EP_MEM:
				record_req(data_record_pkt, mem_wip);
				break;
			case EP_SOC:
			default:
				assert(false);
			}
			out_pkt.pkt = data_record_pkt.pkt;
			out_pkt.last = data_record_pkt.last;
			data_out.write(out_pkt);
			break;

		case DISPATCHER:
			data_pkt = from_dispatch.read();
#ifdef SENDQUEUE_PRINT
			std::cout << "DISP HDR:  " << std::hex << data_pkt.pkt << std::endl;
#endif
			if (data_pkt.last == 0)
				state = DISPATCHER_DATA;

			data_out.write(data_pkt);
			break;

		default:
			break;
		}
		break;

	case PARSER2:
		if (from_parser2.empty())
			break;
		data_record_pkt = from_parser2.read();

		assert(data_record_pkt.last == 1);
		state = READY;

		switch (data_record_pkt.endpoint) {
		case EP_SOC:
			record_req(data_record_pkt, soc_wip);
			break;
		case EP_MEM:
			record_req(data_record_pkt, mem_wip);
			break;
		default:
			assert(false);
		}
		out_pkt.pkt = data_record_pkt.pkt;
		out_pkt.last = data_record_pkt.last;
		data_out.write(out_pkt);
		break;

	case WQ1_MEM_DATA:
		if (from_wq1_mem.empty())
			break;
		data_record_pkt = from_wq1_mem.read();

		if (data_record_pkt.last == 1) {
			port_mask[0] = 0;
			if (port_mask[1] == 1)
				state = WQ1_SOC;
			else if (port_mask[2] == 1)
				state = WQ1_NET;
			else
				state = READY;
		}
		out_pkt.pkt = data_record_pkt.pkt;
		out_pkt.last = data_record_pkt.last;
		data_out.write(out_pkt);
		break;

	case WQ1_SOC:
		if (from_wq1_soc.empty())
			break;
		data_record_pkt = from_wq1_soc.read();

		assert(data_record_pkt.last == 1);
		port_mask[1] = 0;
		if (port_mask[2] == 1)
			state = WQ1_NET;
		else
			state = READY;

		switch (data_record_pkt.endpoint) {
		case EP_SOC:
			record_req(data_record_pkt, soc_wip);
			break;
		case EP_MEM:
		default:
			assert(false);
		}
		out_pkt.pkt = data_record_pkt.pkt;
		out_pkt.last = data_record_pkt.last;
		data_out.write(out_pkt);
		break;

	case WQ1_NET:
		if (from_wq1_net.empty())
			break;
		data_pkt = from_wq1_net.read();

		assert(data_pkt.last == 1);
		port_mask[2] = 0;
		state = READY;

		data_out.write(data_pkt);
		break;

	case WQ2_NET:
		if (from_wq2_net.empty())
			break;
		data_pkt = from_wq2_net.read();

		if (data_pkt.last == 1)
			state = READY;

		data_out.write(data_pkt);
		break;

	case DISPATCHER_DATA:
		if (from_dispatch.empty())
			break;
		data_pkt = from_dispatch.read();
#ifdef SENDQUEUE_PRINT
		std::cout << "DISP DATA: " << std::hex << data_pkt.pkt << std::endl;
#endif

		if (data_pkt.last == 1)
			state = READY;
		data_out.write(data_pkt);
		break;

	default:
		break;
	}
}
