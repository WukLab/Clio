/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR

#include <stdio.h>
#include "rx_64.hpp"

enum udp_recv_status {
	udp_recv_udp_head,
	udp_recv_gbn_head,
	udp_recv_parse_stream
};

/**
 * @rx_header: received udp header from network stack
 * @rx_payload: received udp payload from network stack
 * @rsp_header: udp header for ack/nack response
 * @rsp_payload: udp payload for ack/nack response
 * @ack_header: received ack/nack sent to unack'd queue
 * @ack_payload: received ack/nack sent to unack'd queue
 * @usr_rx_header: udp header sent to onboard pipeline
 * @usr_rx_payload: udp payload sent to onboard pipeline
 */
void rx_64(stream<struct udp_info>	*rx_header,
	   stream<struct net_axis_64>	*rx_payload,
	   stream<struct udp_info>	*rsp_header,
	   stream<struct net_axis_64>	*rsp_payload,
	   stream<struct udp_info>	*ack_header,
	   stream<struct net_axis_64>	*ack_payload,
	   stream<struct udp_info>	*usr_rx_header,
	   stream<struct net_axis_64>	*usr_rx_payload
#ifdef DEBUG_MODE
	   ,ap_uint<1>			reset_seq
#endif
	)
{
/**
 * receive from UDP module
 */
#pragma HLS INTERFACE axis both port=rx_header
#pragma HLS DATA_PACK variable=rx_header
#pragma HLS INTERFACE axis both port=rx_payload
/**
 * send to UDP module
 */
#pragma HLS INTERFACE axis both port=rsp_header
#pragma HLS DATA_PACK variable=rsp_header
#pragma HLS INTERFACE axis both port=rsp_payload

#pragma HLS INTERFACE axis both port=ack_header
#pragma HLS DATA_PACK variable=ack_header
#pragma HLS INTERFACE axis both port=ack_payload

#pragma HLS INTERFACE axis both port=usr_rx_header
#pragma HLS DATA_PACK variable=usr_rx_header
#pragma HLS INTERFACE axis both port=usr_rx_payload

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum udp_recv_status status = udp_recv_udp_head;
	static unsigned int expected_seqnum = 1;

	static struct net_axis_64 recv_pkt, resp_pkt;
	static struct udp_info recv_udp_info, resp_udp_info;

	static bool ack_enable = true;
	static bool deliever_data = false;

#ifdef DEBUG_MODE
	/**
	 * this reset is only used for hls software test,
	 * in hardware we can use the blocklevel reset signal
	 */
	if (reset_seq) {
		expected_seqnum = 1;
		status = udp_recv_udp_head;
		ack_enable = true;
	}
#endif

	switch (status) {
	case udp_recv_udp_head:
		if (rx_header->empty())
			break;
		recv_udp_info = rx_header->read();

		/* generate response header */
		resp_udp_info.dest_ip = recv_udp_info.src_ip;
		resp_udp_info.dest_port = recv_udp_info.src_port;
		resp_udp_info.src_ip = recv_udp_info.dest_ip;
		resp_udp_info.src_port = recv_udp_info.dest_port;
		resp_udp_info.length = 8;

		PR("receive udp header from net: %x:%d -> %x:%d\n",
		   recv_udp_info.src_ip.to_uint(),
		   recv_udp_info.src_port.to_uint(),
		   recv_udp_info.dest_ip.to_uint(),
		   recv_udp_info.dest_port.to_uint());

		status = udp_recv_gbn_head;
		break;
	case udp_recv_gbn_head:
		if (rx_payload->empty())
			break;
		recv_pkt = rx_payload->read();
		PR("receive gbn header: [type %d, seq %lld]\n",
		   recv_pkt.data(7, 0).to_uint(),
		   recv_pkt.data(7 + SEQ_WIDTH, 8).to_uint64());

		resp_pkt.last = 1;
		resp_pkt.keep = 0xff;
		if (recv_pkt.data(7, 0) == pkt_type_data) {

			/**
			 * if received data packet, generate ack/nack response
			 */
			if (recv_pkt.data(7 + SEQ_WIDTH, 8) ==
			    expected_seqnum) {
				ack_enable = true;
				deliever_data = true;

				/* deliever header */
				usr_rx_header->write(recv_udp_info);
				/* generate response packet */
				resp_pkt.data(7, 0) = pkt_type_ack;
				resp_pkt.data(7 + SEQ_WIDTH, 8) = expected_seqnum;
				expected_seqnum++;
			} else if (ack_enable && (recv_pkt.data(7 + SEQ_WIDTH, 8) >
						expected_seqnum)) {
				deliever_data = false;

				resp_pkt.data(7, 0) = pkt_type_nack;
				/* response latest acked seqnum */
				resp_pkt.data(7 + SEQ_WIDTH, 8) =
				    expected_seqnum - 1;
			} else {
				/* received seqnum < expected seqnum */
				deliever_data = false;

				resp_pkt.data(7, 0) = pkt_type_ack;
				/* response latest acked seqnum */
				resp_pkt.data(7 + SEQ_WIDTH, 8) =
				    expected_seqnum - 1;
			}
		} else if (recv_pkt.data(7, 0) == pkt_type_ack ||
			   recv_pkt.data(7, 0) == pkt_type_nack) {

			/**
			 * if received ack/nack, forward it to Uack'd packets queue
			 */
			ack_header->write(resp_udp_info);
			ack_payload->write(recv_pkt);
			deliever_data = false;
		} else {
			deliever_data = false;
		}
		if (recv_pkt.last == 1)
			status = udp_recv_udp_head;
		else
			status = udp_recv_parse_stream;
		break;
	case udp_recv_parse_stream:
		if (rx_payload->empty())
			break;
		recv_pkt = rx_payload->read();
		PR("receive data from net %llx\n", recv_pkt.data.to_uint64());
		if (deliever_data) {

			/**
			 * send data packet to onboard pipeline
			 * ?: what's the interface for onboard pipeline?
			 */
			usr_rx_payload->write(recv_pkt);
		}
		if (recv_pkt.last == 1) {
			status = udp_recv_udp_head;
			deliever_data = false;

			/**
			 * send response udp header and response acknowledgment
			 * ? when to send response? Right after checking seqnum
			 * or until the whole packet is delievered?
			 */
			if (ack_enable) {
				rsp_header->write(resp_udp_info);
				rsp_payload->write(resp_pkt);

				/**
				 * set ack_enable to false when nack(10b)
				 * encoding may change if add more pkt types
				 * ack  = 01b
				 * nack = 10b
				 * data = 11b
				 */
				ack_enable = resp_pkt.data[0];
				PR("response data %llx\n", resp_pkt.data.to_uint64());
			}
		}
		break;
	default:
		break;
	}

}
