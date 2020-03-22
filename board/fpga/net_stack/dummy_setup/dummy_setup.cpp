#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <fpga/rel_net.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

enum status {
	RECV_UDP,
	RECV_PAYLOAD
};

void dummy_setup(stream<struct net_axis_64>	*usr_rx_payload,
		 stream<struct udp_info>	*usr_rx_hdr,
		 stream<struct conn_mgmt_req>	*conn_setup_req)
{
#pragma HLS INTERFACE axis both port=usr_rx_payload
#pragma HLS INTERFACE axis both port=usr_rx_hdr
#pragma HLS INTERFACE axis both port=conn_setup_req

#pragma HLS DATA_PACK variable=conn_setup_req
#pragma HLS DATA_PACK variable=usr_rx_hdr

#pragma HLS PIPELINE
#pragma HLS INTERFACE ap_ctrl_none port=return

	static enum status state = RECV_UDP;

	struct net_axis_64 recv_pkt;
	struct udp_info recv_hdr;
	struct conn_mgmt_req setup_req;

	switch (state) {
	case RECV_UDP:
		if (usr_rx_hdr->empty())
			break;

		recv_hdr = usr_rx_hdr->read();

		if (recv_hdr.src_port > 0 && recv_hdr.dest_port == 0) {
			setup_req.set_type = set_type_setup;
			setup_req.slotid = 10;
			conn_setup_req->write(setup_req);
		} else if (recv_hdr.src_port == 0 && recv_hdr.dest_port > 0) {
			setup_req.set_type = set_type_close;
			setup_req.slotid = recv_hdr.dest_port(SLOT_ID_WIDTH - 1, 0);
			conn_setup_req->write(setup_req);
		}

		state = RECV_PAYLOAD;
		break;
	case RECV_PAYLOAD:
		if (usr_rx_payload->empty())
			break;

		recv_pkt = usr_rx_payload->read();
		if (recv_pkt.last == 1) {
			state = RECV_UDP;
		}
		break;
	}
}
