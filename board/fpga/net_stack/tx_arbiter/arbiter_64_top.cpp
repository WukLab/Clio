/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR

#include "arbiter_64.hpp"

enum udp_arbiter_status {
	udp_arbiter_arb,
	udp_arbiter_send_head,
	udp_arbiter_send_payload
};

enum arb_result {
	arb_rsp,
	arb_rt,
	arb_tx
};

void arbiter_64(stream<struct udp_info>		*rsp_header,
		stream<struct net_axis_64>	*rsp_payload,
		stream<struct udp_info>		*tx_header,
		stream<struct net_axis_64>	*tx_payload,
		stream<struct udp_info>		*rt_header,
		stream<struct net_axis_64>	*rt_payload,
		stream<struct udp_info>		*out_header,
		stream<struct net_axis_64>	*out_payload)
{
#pragma HLS INTERFACE axis both port=rsp_header
#pragma HLS INTERFACE axis both port=rsp_payload
#pragma HLS INTERFACE axis both port=tx_header
#pragma HLS INTERFACE axis both port=tx_payload
#pragma HLS INTERFACE axis both port=rt_header
#pragma HLS INTERFACE axis both port=rt_payload
#pragma HLS INTERFACE axis both port=out_header
#pragma HLS INTERFACE axis both port=out_payload

#pragma HLS DATA_PACK variable=rsp_header
#pragma HLS DATA_PACK variable=tx_header
#pragma HLS DATA_PACK variable=rt_header
#pragma HLS DATA_PACK variable=out_header

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum udp_arbiter_status state = udp_arbiter_arb;
	static enum arb_result result = arb_rsp;

	struct udp_info send_udp_info;
	struct net_axis_64 send_pkt;
	
	switch (state) {
	case udp_arbiter_arb:
		if (!rsp_header->empty()) {
			result = arb_rsp;
			state = udp_arbiter_send_head;
			break;
		} else if (!rt_header->empty()) {
			result = arb_rt;
			state = udp_arbiter_send_head;
			break;
		} else if (!tx_header->empty()) {
			result = arb_tx;
			state = udp_arbiter_send_head;
			break;
		} else {
			break;
		}
	case udp_arbiter_send_head:
		if (result == arb_rsp) {
			send_udp_info = rsp_header->read();
		} else if (result == arb_rt) {
			send_udp_info = rt_header->read();
		} else if (result == arb_tx) {
			send_udp_info = tx_header->read();
		}
		out_header->write(send_udp_info);
		state = udp_arbiter_send_payload;
		break;
	case udp_arbiter_send_payload:
		if (result == arb_rsp) {
			if (rsp_payload->empty()) break;
			send_pkt = rsp_payload->read();
		} else if (result == arb_rt) {
			if (rt_payload->empty()) break;
			send_pkt = rt_payload->read();
		} else if (result == arb_tx) {
			if (tx_payload->empty()) break;
			send_pkt = tx_payload->read();
		}
		out_payload->write(send_pkt);
		if (send_pkt.last == 1) state = udp_arbiter_arb;
		break;
	}
}
