#include "filter_64.hpp"

enum rx_status {
	RX_STATE_UDP_HEADER,
	RX_STATE_DELIVER_DATA
};

void filter_64(stream<struct udp_info>		*rx_header,
	       stream<struct net_axis_64>	*rx_payload,
	       stream<struct udp_info>		*usr_rx_header,
	       stream<struct net_axis_64>	*usr_rx_payload)
{
#pragma HLS INTERFACE axis both port=rx_header
#pragma HLS INTERFACE axis both port=rx_payload
#pragma HLS INTERFACE axis both port=usr_rx_header
#pragma HLS INTERFACE axis both port=usr_rx_payload

#pragma HLS DATA_PACK variable=rx_header
#pragma HLS DATA_PACK variable=usr_rx_header

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum rx_status state = RX_STATE_UDP_HEADER;

	static struct net_axis_64 recv_pkt;
	static struct udp_info recv_udp_info;
	static bool deliver_data = false;

	switch (state) {
	case RX_STATE_UDP_HEADER:
		if (rx_header->empty() || rx_payload->empty())
			break;
		recv_udp_info = rx_header->read();
		
		if (recv_udp_info.dest_ip(7, 0) == 0xff) {
			deliver_data = false;
			state = RX_STATE_DELIVER_DATA;
			break;
		}

		deliver_data = true;
		usr_rx_header->write(recv_udp_info);
		state = RX_STATE_DELIVER_DATA;
		break;
	case RX_STATE_DELIVER_DATA:
		if (rx_payload->empty())
			break;

		recv_pkt = rx_payload->read();

		if (deliver_data)
			usr_rx_payload->write(recv_pkt);
		if (recv_pkt.last == 1) {
			state = RX_STATE_UDP_HEADER;
			deliver_data = false;
		}
		break;
	default:
		break;
	}
}

