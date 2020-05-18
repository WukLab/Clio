/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR
#define ENABLE_PROBE

#include "tx_64.hpp"

enum udp_send_status {
	TX_STATE_UDP_HEADER,
	TX_STATE_GBN_HEADER,
	TX_STATE_PAYLOAD
};

/**
 * @tx_header: udp header sent to network stack
 * @tx_payload: udp payload sent to network stack
 * @usr_tx_header: received udp header from onboard pipeline
 * @usr_tx_payload: received udp payload from onboard pipeline
 * @check_full_req: request to check if buffer for this flow is full
 * @check_full_res: returned last sent seqnum for this flow
 * @tx_finish_sig: signal for inform completion of a packet transfer
 * @tx_buff_payload: udp payload sent to nacked buffer
 */
void tx_64(stream<struct udp_info>		*tx_header,
	   stream<struct net_axis_64>		*tx_payload,
	   stream<struct udp_info>		*usr_tx_header,
	   stream<struct net_axis_64>		*usr_tx_payload,
	   stream<ap_uint<SLOT_ID_WIDTH> >	*check_full_req,
	   stream<ap_uint<SEQ_WIDTH> >		*check_full_rsp,
	   stream<bool>				*tx_finish_sig,
	   stream<struct net_axis_64>		*tx_buff_payload,
	   stream<struct route_info>		*tx_buff_route_info
#ifdef ENABLE_PROBE
	   ,volatile enum udp_send_status	*send_state
	   ,volatile unsigned int		*tx_cnt
#endif
)
{
#pragma HLS INTERFACE axis both port=tx_header
#pragma HLS INTERFACE axis both port=tx_payload
#pragma HLS INTERFACE axis both port=usr_tx_header
#pragma HLS INTERFACE axis both port=usr_tx_payload
#pragma HLS INTERFACE axis both port=check_full_req
#pragma HLS INTERFACE axis both port=check_full_rsp
#pragma HLS INTERFACE axis both port=tx_finish_sig
#pragma HLS INTERFACE axis both port=tx_buff_payload
#pragma HLS INTERFACE axis both port=tx_buff_route_info
#ifdef ENABLE_PROBE
#pragma HLS INTERFACE ap_none port=send_state
#pragma HLS INTERFACE ap_none port=tx_cnt
#endif

#pragma HLS DATA_PACK variable=tx_header
#pragma HLS DATA_PACK variable=usr_tx_header
#pragma HLS DATA_PACK variable=tx_buff_route_info

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum udp_send_status state = TX_STATE_UDP_HEADER;

	static struct udp_info send_udp_info;
	struct route_info send_route_info;
	struct net_axis_64 send_pkt;

	static struct net_axis_64 gbn_header;
	static ap_uint<SLOT_ID_WIDTH> slot_id;
	static ap_uint<SEQ_WIDTH> tx_ck_rsp;
	static bool if_buffer = true;

#ifdef ENABLE_PROBE
	static unsigned int nr_tx = 0;
#endif

	switch (state) {
	case TX_STATE_UDP_HEADER:
		if (usr_tx_header->empty())
			break;

		send_udp_info = usr_tx_header->read();
		PR("get header from MMU: %x:%d -> %x:%d\n",
		   send_udp_info.src_ip.to_uint(),
		   send_udp_info.src_port.to_uint(),
		   send_udp_info.dest_ip.to_uint(),
		   send_udp_info.dest_port.to_uint());

		/*
		 * cook the session ID from the udp header
		 * src_port -> src slot id
		 * dest_port -> dest slot id
		 */
		gbn_header.data(SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET) =
			send_udp_info.src_port(SLOT_ID_WIDTH - 1, 0);
		gbn_header.data(DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET) =
			send_udp_info.dest_port(SLOT_ID_WIDTH - 1, 0);
		gbn_header.data(PKT_TYPE_OFFSET + PKT_TYPE_WIDTH - 1, PKT_TYPE_OFFSET) = GBN_PKT_DATA;

		send_udp_info.length += 16;  // add the length of gbn header and udp header

		if (send_udp_info.src_port > 0 && send_udp_info.dest_port > 0) {
			slot_id = send_udp_info.src_port(SLOT_ID_WIDTH - 1, 0);

			send_udp_info.src_port = LEGOMEM_PORT;
			send_udp_info.dest_port = LEGOMEM_PORT;

			check_full_req->write(slot_id);
			state = TX_STATE_GBN_HEADER;
		} else {
			/*
			 * Either src port (src slot id) or dest port (dest slot id)
			 * is 0. This packet is from session 0. It's used for connection
			 * management. Do not buffer it and diectly send it out.
			 */
			gbn_header.data(SEQ_OFFSET + SEQ_WIDTH - 1,
					SEQ_OFFSET) = 0;
			gbn_header.keep = 0xff;
			gbn_header.last = 0;

			send_udp_info.src_port = LEGOMEM_PORT;
			send_udp_info.dest_port = LEGOMEM_PORT;

			tx_header->write(send_udp_info);
			tx_payload->write(gbn_header);

			/* do not buffer SYN and FIN packet */
			if_buffer = false;
			state = TX_STATE_PAYLOAD;
		}

		break;
	case TX_STATE_GBN_HEADER:
		if (check_full_rsp->empty())
			break;
		tx_ck_rsp = check_full_rsp->read();
		PR("get seqnum from state table: %ld\n", tx_ck_rsp.to_uint());

		/*
		 * cook other fields in the gbn header
		 */
		gbn_header.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET) =
			tx_ck_rsp;
		gbn_header.keep = 0xff;
		gbn_header.last = 0;

		/*
		 * We will encode the src ip at the udp layer, since
		 * the ip for each board is pre-determined, so we don't
		 * need to take care of it at GBN layer
		 */
		send_route_info.dest_ip = send_udp_info.dest_ip;
		send_route_info.length = send_udp_info.length;

		/*
		 * send udp header to tx port
		 * send route info to buffer
		 * send gbn header to tx port and buffer
		 */
		tx_header->write(send_udp_info);
		tx_buff_route_info->write(send_route_info);

		tx_payload->write(gbn_header);
		tx_buff_payload->write(gbn_header);

		/* buffer data packet */
		if_buffer = true;
		state = TX_STATE_PAYLOAD;
		break;
	case TX_STATE_PAYLOAD:
		if (usr_tx_payload->empty())
			break;
		send_pkt = usr_tx_payload->read();
		PR("get payload from MMU: %llx\n", send_pkt.data.to_uint64());

		/*
		 * send udp payload to tx port and buffer
		 */
		tx_payload->write(send_pkt);
		if (if_buffer)
			tx_buff_payload->write(send_pkt);

		if (send_pkt.last == 1) {
			state = TX_STATE_UDP_HEADER;
#ifdef ENABLE_PROBE
			nr_tx++;
#endif
			/*
			 * send a signal to state table to
			 * inform the completion of a packet transfer
			 */
			if (if_buffer) {
				tx_finish_sig->write(true);
			}
			if_buffer = true;
		}

		break;
	default:
		break;
	}
#ifdef ENABLE_PROBE
	*send_state = state;
	*tx_cnt = nr_tx;
#endif
}
