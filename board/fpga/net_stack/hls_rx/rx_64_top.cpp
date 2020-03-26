/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR

#include <stdio.h>
#include "rx_64.hpp"

enum udp_recv_status {
	RX_STATE_UDP_HEADER,
	RX_STATE_GBN_HEADER,
	RX_STATE_WAIT_RSP,
	RX_STATE_DELIVER_DATA
};

/**
 * @rx_header: received udp header from network stack
 * @rx_payload: received udp payload from network stack
 * @state_query_req: gbn header sent to state table for query
 * @state_query_rsp: query response from state table
 * @usr_rx_header: udp header sent to onboard pipeline
 * @usr_rx_payload: udp payload sent to onboard pipeline
 */
void rx_64(stream<struct udp_info>	*rx_header,
	   stream<struct net_axis_64>	*rx_payload,
	   stream<struct query_req>	*state_query_req,
	   stream<bool>			*state_query_rsp,
	   stream<struct udp_info>	*usr_rx_header,
	   stream<struct net_axis_64>	*usr_rx_payload)
{
#pragma HLS INTERFACE axis both port=rx_header
#pragma HLS DATA_PACK variable=rx_header
#pragma HLS INTERFACE axis both port=rx_payload

#pragma HLS INTERFACE axis both port=state_query_req
#pragma HLS DATA_PACK variable=state_query_req
#pragma HLS INTERFACE axis both port=state_query_rsp

#pragma HLS INTERFACE axis both port=usr_rx_header
#pragma HLS DATA_PACK variable=usr_rx_header
#pragma HLS INTERFACE axis both port=usr_rx_payload

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum udp_recv_status state = RX_STATE_UDP_HEADER;

	static struct net_axis_64 recv_pkt;
	static struct udp_info recv_udp_info;
	static struct query_req gbn_query_req;

	static bool deliver_data = false;

	switch (state) {
	case RX_STATE_UDP_HEADER:
		if (rx_header->empty())
			break;
		recv_udp_info = rx_header->read();

		PR("receive udp header from net: %x:%d -> %x:%d\n",
		   recv_udp_info.src_ip.to_uint(),
		   recv_udp_info.src_port.to_uint(),
		   recv_udp_info.dest_ip.to_uint(),
		   recv_udp_info.dest_port.to_uint());

		/*
		 * src/dest ip are reversed in state table
		 */
		gbn_query_req.src_ip = recv_udp_info.src_ip;
		gbn_query_req.dest_ip = recv_udp_info.dest_ip;

		state = RX_STATE_GBN_HEADER;
		break;
	case RX_STATE_GBN_HEADER:
		if (rx_payload->empty())
			break;
		recv_pkt = rx_payload->read();

		PR("receive gbn header: [type %d, seq %lld, src slot %d, dest slot %d]\n",
		   recv_pkt.data(PKT_TYPE_WIDTH - 1, 0).to_uint(),
		   recv_pkt.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET).to_uint64(),
		   recv_pkt.data(SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
				 SRC_SLOT_OFFSET).to_uint(),
		   recv_pkt.data(DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
				 DEST_SLOT_OFFSET).to_uint());

		gbn_query_req.gbn_header = recv_pkt.data;
		state_query_req->write(gbn_query_req);
		/*
		 * keep the interface to downstream coreMem unchanged,
		 * src ip and dest ip are still needed, port is not needed
		 * so currently just put the slot id in the port
		 * fields of udp header.
		 */
		recv_udp_info.dest_port = recv_pkt.data(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		recv_udp_info.src_port = recv_pkt.data(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		// subtract the length of gbn header and udp header
		recv_udp_info.length -= 16;

		state = RX_STATE_WAIT_RSP;
		break;
	case RX_STATE_WAIT_RSP:
		if (state_query_rsp->empty())
			break;
		deliver_data = state_query_rsp->read();

		if (recv_pkt.last == 1)
			state = RX_STATE_UDP_HEADER;
		else {
			if (deliver_data)
				usr_rx_header->write(recv_udp_info);
			state = RX_STATE_DELIVER_DATA;
		}
		break;
	case RX_STATE_DELIVER_DATA:
		if (rx_payload->empty())
			break;
		recv_pkt = rx_payload->read();

		PR("receive data from net %llx\n", recv_pkt.data.to_uint64());

		if (deliver_data) {
			usr_rx_payload->write(recv_pkt);
		}
		if (recv_pkt.last == 1) {
			state = RX_STATE_UDP_HEADER;
			deliver_data = false;
		}
		break;
	default:
		break;
	}
}
