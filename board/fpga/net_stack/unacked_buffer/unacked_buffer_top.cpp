#include "unacked_buffer.hpp"

enum buff_recv_status {
	BUF_STATE_RECV_ROUTE_INFO,
	BUF_STATE_RECV_GBN_HEADER,
	BUF_STATE_RECV_PAYLOAD
};

enum buff_retrans_status {
	BUF_STATE_RETRANS_HEADER,
	BUF_STATE_RETRANS_PAYLOAD
};

void unacked_buffer(stream<struct timer_req>	*timer_rst_req,
		    stream<struct net_axis_64>	*tx_buff_payload,
		    stream<struct route_info>	*tx_buff_route_info,
		    stream<struct retrans_req>	*gbn_retrans_req)
{
#pragma HLS INTERFACE axis both port=timer_rst_req
#pragma HLS INTERFACE axis both port=tx_buff_payload
#pragma HLS INTERFACE axis both port=tx_buff_route_info
#pragma HLS INTERFACE axis both port=gbn_retrans_req

#pragma HLS DATA_PACK variable=timer_rst_req
#pragma HLS DATA_PACK variable=tx_buff_route_info
#pragma HLS DATA_PACK variable=gbn_retrans_req

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum buff_recv_status recv_state = BUF_STATE_RECV_ROUTE_INFO;
	static enum buff_retrans_status retrans_state =	BUF_STATE_RETRANS_HEADER;

	struct net_axis_64 recv_packet;
	struct route_info recv_route_info;
	struct timer_req rst_timer_req;
	struct retrans_req gbn_rt_req;

	switch (recv_state) {
	case BUF_STATE_RECV_ROUTE_INFO:
		if (tx_buff_route_info->empty())
			break;

		recv_route_info = tx_buff_route_info->read();
		recv_state = BUF_STATE_RECV_GBN_HEADER;
		break;
	case BUF_STATE_RECV_GBN_HEADER:
		if (tx_buff_payload->empty())
			break;
		recv_packet = tx_buff_payload->read();
		recv_state = BUF_STATE_RECV_PAYLOAD;
		break;
	case BUF_STATE_RECV_PAYLOAD:
		if (tx_buff_payload->empty())
			break;
		recv_packet = tx_buff_payload->read();
		if (recv_packet.last == 1) {
			recv_state = BUF_STATE_RECV_ROUTE_INFO;
		}
		break;
	default:
		break;
	}

	switch (retrans_state) {
	case BUF_STATE_RETRANS_HEADER:
		if (gbn_retrans_req->empty())
			break;
		gbn_rt_req = gbn_retrans_req->read();
		retrans_state = BUF_STATE_RETRANS_PAYLOAD;
		break;
	case BUF_STATE_RETRANS_PAYLOAD:
		rst_timer_req.rst_type = timer_rst_type_reset;
		rst_timer_req.slotid = 0;
		timer_rst_req->write(rst_timer_req);
		retrans_state = BUF_STATE_RETRANS_HEADER;
		break;
	default:
		break;
	}
}
