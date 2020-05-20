/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <tb_sim.h>
#include <stdexcept>
#include <iostream>

using namespace std;
using namespace hls;

XbarSim::XbarSim()
{
	state = XBAR_HEADER;
	data_remain = 0;
	dest = 0;
	cout << "Simulated Crossbar Initialized" << endl;
}

void XbarSim::xbar_sim(stream<struct lego_mem_ctrl> &to_extep_ctrl,
		       stream<struct lego_mem_ctrl> &from_extep_ctrl,
		       stream<struct lego_mem_ctrl> &to_soc_ctrl,
		       stream<struct lego_mem_ctrl> &from_soc_ctrl,
		       stream<ap_uint<DATAWIDTH> > &to_net_data,
		       stream<ap_uint<DATAWIDTH> > &from_net_data,
		       stream<ap_uint<DATAWIDTH> > &to_extep_data,
		       stream<ap_uint<DATAWIDTH> > &from_extep_data,
		       stream<ap_uint<DATAWIDTH> > &to_soc_data,
		       stream<ap_uint<DATAWIDTH> > &from_soc_data,
		       stream<ap_uint<DATAWIDTH> > &to_mem_data,
		       stream<ap_uint<DATAWIDTH> > &from_mem_data)
{
	ap_uint<DATAWIDTH> data_pkt;

	if (!from_extep_ctrl.empty()) {
		to_soc_ctrl.write(from_extep_ctrl.read());
	}

	if (!from_soc_ctrl.empty()) {
		to_extep_ctrl.write(from_soc_ctrl.read());
	}

	switch (state) {
	case XBAR_HEADER:
		if (!from_soc_data.empty()) {
			data_pkt = from_soc_data.read();
			assert(field(data_pkt, hdr_size) <= DATASIZE);
			switch (field(data_pkt, hdr_cont)) {
			case LEGOMEM_CONT_NET:
				to_net_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_MEM:
				to_mem_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_SOC:
				throw invalid_argument("bad cont: from soc to soc");
				break;
			case LEGOMEM_CONT_EXTAPI:
				to_extep_data.write(data_pkt);
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		} else if (!from_mem_data.empty()) {
			data_pkt = from_mem_data.read();
			if (field(data_pkt, hdr_size) > DATASIZE) {
				state = XBAR_MEM_DATA;
				data_remain = field(data_pkt, hdr_size) - DATASIZE;
				dest = field(data_pkt, hdr_cont);
			}
			switch (field(data_pkt, hdr_cont)) {
			case LEGOMEM_CONT_NET:
				to_net_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_MEM:
				throw invalid_argument("bad cont: from mem to mem");
				break;
			case LEGOMEM_CONT_SOC:
				to_soc_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_EXTAPI:
				to_extep_data.write(data_pkt);
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		} else if (!from_extep_data.empty()) {
			data_pkt = from_extep_data.read();
			if (field(data_pkt, hdr_size) > DATASIZE) {
				state = XBAR_EXTEP_DATA;
				data_remain = field(data_pkt, hdr_size) - DATASIZE;
				dest = field(data_pkt, hdr_cont);
			}
			switch (field(data_pkt, hdr_cont)) {
			case LEGOMEM_CONT_NET:
				to_net_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_MEM:
				to_mem_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_SOC:
				to_soc_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_EXTAPI:
				throw invalid_argument("bad cont: from extep to extep");
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		} else if (!from_net_data.empty()) {
			data_pkt = from_net_data.read();
			if (field(data_pkt, hdr_size) > DATASIZE) {
				state = XBAR_NET_DATA;
				data_remain = field(data_pkt, hdr_size) - DATASIZE;
				dest = field(data_pkt, hdr_cont);
			}
			switch (field(data_pkt, hdr_cont)) {
			case LEGOMEM_CONT_NET:
				throw invalid_argument("bad cont: from net to net");
				break;
			case LEGOMEM_CONT_MEM:
				to_mem_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_SOC:
				to_soc_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_EXTAPI:
				to_extep_data.write(data_pkt);
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		}
		break;

	case XBAR_NET_DATA:
		if (!from_net_data.empty()) {
			data_pkt = from_net_data.read();
			if (data_remain > DATASIZE) {
				data_remain -= DATASIZE;
			} else {
				data_remain = 0;
				state = XBAR_HEADER;
			}
			switch (dest) {
			case LEGOMEM_CONT_NET:
				throw invalid_argument("bad cont: from net to net");
				break;
			case LEGOMEM_CONT_MEM:
				to_mem_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_SOC:
				to_soc_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_EXTAPI:
				to_extep_data.write(data_pkt);
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		}
		break;

	case XBAR_MEM_DATA:
		if (!from_mem_data.empty()) {
			data_pkt = from_mem_data.read();
			if (data_remain > DATASIZE) {
				data_remain -= DATASIZE;
			} else {
				data_remain = 0;
				state = XBAR_HEADER;
			}
			switch (dest) {
			case LEGOMEM_CONT_NET:
				to_net_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_MEM:
				throw invalid_argument("bad cont: from mem to mem");
				break;
			case LEGOMEM_CONT_SOC:
				to_soc_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_EXTAPI:
				to_extep_data.write(data_pkt);
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		}
		break;

	case XBAR_EXTEP_DATA:
		if (!from_extep_data.empty()) {
			data_pkt = from_extep_data.read();
			if (data_remain > DATASIZE) {
				data_remain -= DATASIZE;
			} else {
				data_remain = 0;
				state = XBAR_HEADER;
			}
			switch (dest) {
			case LEGOMEM_CONT_NET:
				to_net_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_MEM:
				to_mem_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_SOC:
				to_soc_data.write(data_pkt);
				break;
			case LEGOMEM_CONT_EXTAPI:
				throw invalid_argument("bad cont: from extep to extep");
				break;
			default:
				throw invalid_argument("bad cont: unknown");
				break;
			}
		}
		break;

	default:
		break;
	}
}
