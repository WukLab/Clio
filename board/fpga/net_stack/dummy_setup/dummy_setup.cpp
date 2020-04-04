#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <uapi/gbn.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

enum LEGOFPGA_OPCODE_REQ {
	OP_REQ_TEST = 0,

	OP_REQ_ALLOC = 1,
	OP_REQ_FREE,

	OP_REQ_READ,
	OP_REQ_WRITE,

	OP_CREATE_PROC,
	OP_FREE_PROC,

	OP_OPEN_SESSION,
	OP_CLOSE_SESSION,

	OP_REQ_MIGRATION,

	/* Host to Monitor */
	OP_REQ_MEMBERSHIP_JOIN_CLUSTER,

	OP_REQ_MEMBERSHIP_NEW_NODE,

	OP_RESET_ALL,

	OP_REQ_SOC_DEBUG,
	OP_REQ_FPGA_PINGPOING,	/* For measurement */
	OP_REQ_SOC_PINGPONG,	/* For measurement */
};

enum status {
	RECV_UDP,
	RECV_LEGOHDR,
	RECV_SESID,
	RECV_PAYLOAD
};

void dummy_setup(stream<struct net_axis_64>	*usr_rx_payload,
		 stream<struct udp_info>	*usr_rx_hdr,
		 stream<struct conn_mgmt_req>	*conn_setup_req,
		 stream<struct net_axis_64>	*usr_tx_payload,
		 stream<struct udp_info>	*usr_tx_hdr,
		 unsigned int			*recv_cnt)
{
#pragma HLS INTERFACE axis both port=usr_rx_payload
#pragma HLS INTERFACE axis both port=usr_rx_hdr
#pragma HLS INTERFACE axis both port=usr_tx_payload
#pragma HLS INTERFACE axis both port=usr_tx_hdr
#pragma HLS INTERFACE axis both port=conn_setup_req

#pragma HLS DATA_PACK variable=conn_setup_req
#pragma HLS DATA_PACK variable=usr_rx_hdr
#pragma HLS DATA_PACK variable=usr_tx_hdr

#pragma HLS PIPELINE
#pragma HLS INTERFACE ap_ctrl_none port=return

	static enum status state = RECV_UDP;

	struct net_axis_64 recv_pkt, resp_pkt;
	static struct udp_info recv_hdr, resp_hdr;
	struct conn_mgmt_req setup_req;
	ap_uint<SLOT_ID_WIDTH> session_id;
	static short op_code;
	static unsigned int cnt = 0;

	switch (state) {
	case RECV_UDP:
		if (usr_rx_hdr->empty())
			break;

		recv_hdr = usr_rx_hdr->read();

		resp_hdr.dest_ip = recv_hdr.src_ip;
		resp_hdr.src_ip = recv_hdr.dest_ip;
		resp_hdr.src_port = recv_hdr.dest_port;
		resp_hdr.dest_port = recv_hdr.src_port;
		resp_hdr.length = recv_hdr.length;

		state = RECV_LEGOHDR;
		break;
	case RECV_LEGOHDR:
		if (usr_rx_payload->empty())
			break;
		recv_pkt = usr_rx_payload->read();

		op_code = recv_pkt.data(47, 32);
		if (op_code == OP_OPEN_SESSION ||
		    op_code == OP_CLOSE_SESSION) {
			state = RECV_SESID;
			usr_tx_hdr->write(resp_hdr);
			usr_tx_payload->write(recv_pkt);
		} else {
			cnt++;
			*recv_cnt = cnt;
			if (recv_pkt.last == 1)
				state = RECV_UDP;
			else
				state = RECV_PAYLOAD;
		}
		break;
	case RECV_SESID:
		if (usr_rx_payload->empty())
			break;
		recv_pkt = usr_rx_payload->read();
		resp_pkt = recv_pkt;
		resp_pkt.last = 1;

		session_id = recv_pkt.data(SLOT_ID_WIDTH - 1, 0);
		if (op_code == OP_OPEN_SESSION) {
			setup_req.set_type = set_type_open;
			setup_req.slotid = session_id;
			conn_setup_req->write(setup_req);
		} else if (op_code == OP_CLOSE_SESSION) {
			setup_req.set_type = set_type_close;
			setup_req.slotid = session_id;
			conn_setup_req->write(setup_req);
			resp_pkt.data(SLOT_ID_WIDTH - 1, 0) = 0;
		}

		usr_tx_payload->write(resp_pkt);

		if (recv_pkt.last == 1)
			state = RECV_UDP;
		else
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
