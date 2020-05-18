/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR
// #define ENABLE_PROBE

#include "arbiter_64.hpp"

enum udp_arbiter_status {
	udp_arbiter_arb,
	udp_arb_send_payload_rsp,
	udp_arb_send_payload_rsp_tail,
	udp_arb_send_payload_rt,
	udp_arb_send_payload_tx
};

/**
 * @rsp_header: udp header for ack/nack response
 * @rsp_payload: udp payload for ack/nack response
 * @tx_header: udp header sent to network stack
 * @tx_payload: udp payload sent to network stack
 * @rt_header: retransmit udp header
 * @rt_payload: retransmit udp payload
 * @out_header: final header interface to network stack
 * @out_payload: final payload interface to network stack
 * priority:
 * 1. rsp
 * 2. retrans
 * 3. normal trans
 * TODO: configurable priority
 */
void arbiter_64(stream<struct udp_info>		*rsp_header,
		stream<struct net_axis_64>	*rsp_payload,
		stream<struct udp_info>		*tx_header,
		stream<struct net_axis_64>	*tx_payload,
		stream<struct udp_info>		*rt_header,
		stream<struct net_axis_64>	*rt_payload,
		stream<struct udp_info>		*out_header,
		stream<struct net_axis_64>	*out_payload
#ifdef ENABLE_PROBE
		,volatile unsigned int 		*send_cnt
		,volatile unsigned int		*rsp_cnt
		,volatile unsigned int		*rt_cnt
		,volatile enum udp_arbiter_status *arb_state
#endif
)
{
#pragma HLS INTERFACE axis both port=rsp_header
#pragma HLS INTERFACE axis both port=rsp_payload
#pragma HLS INTERFACE axis both port=tx_header
#pragma HLS INTERFACE axis both port=tx_payload
#pragma HLS INTERFACE axis both port=rt_header
#pragma HLS INTERFACE axis both port=rt_payload
#pragma HLS INTERFACE axis both port=out_header
#pragma HLS INTERFACE axis both port=out_payload

#ifdef ENABLE_PROBE
#pragma HLS INTERFACE ap_none port=send_cnt
#pragma HLS INTERFACE ap_none port=rsp_cnt
#pragma HLS INTERFACE ap_none port=rt_cnt
#pragma HLS INTERFACE ap_none port=arb_state
#endif

#pragma HLS DATA_PACK variable=rsp_header
#pragma HLS DATA_PACK variable=tx_header
#pragma HLS DATA_PACK variable=rt_header
#pragma HLS DATA_PACK variable=out_header

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum udp_arbiter_status state = udp_arbiter_arb;

	struct udp_info send_udp_info;
	struct net_axis_64 send_pkt, ack_tail;

#ifdef ENABLE_PROBE
	static unsigned int nr_send = 0;
	static unsigned int nr_rt = 0;
	static unsigned int nr_rsp = 0;
#endif

	switch (state) {
	case udp_arbiter_arb:
		if (!rsp_header->empty()) {
			send_udp_info = rsp_header->read();
			out_header->write(send_udp_info);
			state = udp_arb_send_payload_rsp;
			break;
		} else if (!rt_header->empty()) {
			send_udp_info = rt_header->read();
			out_header->write(send_udp_info);
			state = udp_arb_send_payload_rt;
			break;
		} else if (!tx_header->empty()) {
			send_udp_info = tx_header->read();
			out_header->write(send_udp_info);
			state = udp_arb_send_payload_tx;
			break;
		} else {
			break;
		}
	case udp_arb_send_payload_rsp:
		if (rsp_payload->empty())
			break;
		send_pkt = rsp_payload->read();
		out_payload->write(send_pkt);
		state = udp_arb_send_payload_rsp_tail;
		break;
	case udp_arb_send_payload_rsp_tail:
		ack_tail.data = 0;
		ack_tail.keep = 0xff;
		ack_tail.last = 1;
		out_payload->write(ack_tail);
#ifdef ENABLE_PROBE
		nr_rsp++;
#endif
		state = udp_arbiter_arb;
		break;
	case udp_arb_send_payload_rt:
		if (rt_payload->empty())
			break;
		send_pkt = rt_payload->read();
		out_payload->write(send_pkt);
		if (send_pkt.last == 1) {
#ifdef ENABLE_PROBE
			nr_rt++;
#endif
			state = udp_arbiter_arb;
		}
		break;
	case udp_arb_send_payload_tx:
		if (tx_payload->empty())
			break;
		send_pkt = tx_payload->read();
		out_payload->write(send_pkt);
		if (send_pkt.last == 1) {
#ifdef ENABLE_PROBE
			nr_send++;
#endif
			state = udp_arbiter_arb;
		}
		break;
	default:
		break;
	}
#ifdef ENABLE_PROBE
	*send_cnt = nr_send;
	*rt_cnt = nr_rt;
	*rsp_cnt = nr_rsp;
	*arb_state = state;
#endif
}
