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
	dest = 0;
	cout << "Simulated Crossbar Initialized" << endl;
}

void XbarSim::xbar_sim(stream<struct ctrl_if> &to_extep_ctrl,
		       stream<struct ctrl_if> &from_extep_ctrl,
		       stream<struct ctrl_if> &to_soc_ctrl,
		       stream<struct ctrl_if> &from_soc_ctrl,
		       stream<struct data_if> &to_net_data,
		       stream<struct data_if> &from_net_data,
		       stream<struct data_if> &to_extep_data,
		       stream<struct data_if> &from_extep_data,
		       stream<struct data_if> &to_soc_data,
		       stream<struct data_if> &from_soc_data,
		       stream<struct data_if> &to_mem_data,
		       stream<struct data_if> &from_mem_data)
{
	struct data_if data_pkt;

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
			assert(data_pkt.last == 1);
			switch (field(data_pkt.pkt, hdr_cont)) {
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
			if (data_pkt.last == 0) {
				state = XBAR_MEM_DATA;
				dest = field(data_pkt.pkt, hdr_cont);
			}
			switch (field(data_pkt.pkt, hdr_cont)) {
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
			if (data_pkt.last == 0) {
				state = XBAR_EXTEP_DATA;
				dest = field(data_pkt.pkt, hdr_cont);
			}
			switch (field(data_pkt.pkt, hdr_cont)) {
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
			if (data_pkt.last == 0) {
				state = XBAR_NET_DATA;
				dest = field(data_pkt.pkt, hdr_cont);
			}
			switch (field(data_pkt.pkt, hdr_cont)) {
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
			if (data_pkt.last == 1)
				state = XBAR_HEADER;
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
			if (data_pkt.last == 1)
				state = XBAR_HEADER;
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
			if (data_pkt.last == 1)
				state = XBAR_HEADER;
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
