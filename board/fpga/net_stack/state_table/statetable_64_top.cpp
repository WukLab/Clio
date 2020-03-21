/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR

#include <stdio.h>
#include "statetable_64.hpp"

enum table_handle_rx_status {
	TAB_STATE_RECV_RX_REQ,
	TAB_STATE_HANDLE_DATA,
	TAB_STATE_HANDLE_ACK
};

enum table_handle_tx_status {
	TAB_STATE_RECV_TX_REQ,
	TAB_STATE_REPL_TX_REQ,
	TAB_STATE_UPD_TX_INFO
};

/**
 * @rsp_header: udp header for ack/nack response
 * @rsp_payload: udp payload for ack/nack response
 * @state_query_req: gbn header from rx module for query
 * @state_query_rsp: query response to rx module
 * @timer_rst_req: timer reset request to timer
 * @rt_timeout_sig: timeout signal from timer
 * @tx_finish_sig: signal for inform completion of a packet transfer
 * @check_full_req: request to check if buffer for this flow is full
 * @check_full_res: returned last sent seqnum for this flow
 * @init_req: initiate states for one connection
 */
void state_table_64(stream<struct udp_info>		*rsp_header,
		    stream<struct net_axis_64>		*rsp_payload,
		    stream<struct query_req>		*state_query_req,
		    stream<bool>			*state_query_rsp,
		    stream<struct retrans_req>		*gbn_retrans_req,
		    stream<struct timer_req>		*timer_rst_req,
		    stream<ap_uint<SLOT_ID_WIDTH> >	*rt_timeout_sig,
		    stream<bool>			*tx_finish_sig,
		    stream<ap_uint<SLOT_ID_WIDTH> >	*check_full_req,
		    stream<ap_uint<SEQ_WIDTH> >		*check_full_rsp,
		    stream<ap_uint<SLOT_ID_WIDTH> >	*init_req)
{
#pragma HLS INTERFACE axis both port=rsp_header
#pragma HLS DATA_PACK variable=rsp_header
#pragma HLS INTERFACE axis both port=rsp_payload

#pragma HLS INTERFACE axis both port=state_query_req
#pragma HLS DATA_PACK variable=state_query_req
#pragma HLS INTERFACE axis both port=state_query_rsp

#pragma HLS INTERFACE axis both port=gbn_retrans_req
#pragma HLS INTERFACE axis both port=timer_rst_req
#pragma HLS INTERFACE axis both port=rt_timeout_sig
#pragma HLS INTERFACE axis both port=tx_finish_sig
#pragma HLS INTERFACE axis both port=check_full_req
#pragma HLS INTERFACE axis both port=check_full_rsp
#pragma HLS INTERFACE axis both port=init_req

#pragma HLS DATA_PACK variable=gbn_retrans_req
#pragma HLS DATA_PACK variable=timer_rst_req

/*
 * It's very hard to achieve II=1 since we need to read and write BRAM.
 * However we don't need II=1, since receiveing and sending
 * a packet takes many cycles, this module is not called on every cycle 
 */
#pragma HLS PIPELINE II=2
#pragma HLS INTERFACE ap_ctrl_none port=return

	static struct query_req gbn_query_req;
	static struct udp_info rsp_udp_info;
	static struct net_axis_64 rsp_pkt;

	struct retrans_req gbn_rt_req;
	struct timer_req rst_timer_req;
	bool deliver_data;
	static bool retrans_data = false;

	/*
	 * seqnum info
	 *
	 * 		 |<-----window----->|
	 * ++++++++++++++|----------********|
	 * 		^	   ^
	 * 	last_ackd_seqnum   |
	 * 		    last_sent_seqnum
	 */
	static ap_uint<SEQ_WIDTH> recv_seqnum, expt_seqnum, rx_last_ackd_seqnum,
		rx_last_sent_seqnum;
	static ap_uint<SLOT_ID_WIDTH> rx_slot_id, tx_slot_id, rt_slot_id;

	static enum table_handle_rx_status handle_rx_state =
		TAB_STATE_RECV_RX_REQ;
	static enum table_handle_tx_status handle_tx_state =
		TAB_STATE_RECV_TX_REQ;

	/*
	 * These are connection states
	 * Need to be initialized when a connection is setup
	 */
	static bool ack_enable_bitmap[MAX_NR_CONN];
	static ap_uint<SEQ_WIDTH> expected_seqnum_array[MAX_NR_CONN];
	static ap_uint<SEQ_WIDTH> last_ackd_seqnum_array[MAX_NR_CONN];
	static ap_uint<SEQ_WIDTH> last_sent_seqnum_array[MAX_NR_CONN];
#pragma HLS DEPENDENCE variable=ack_enable_bitmap false
#pragma HLS DEPENDENCE variable=expected_seqnum_array false
#pragma HLS DEPENDENCE variable=last_acked_seqnum_array false
#pragma HLS DEPENDENCE variable=last_sent_seqnum_array false

	/*
	 * state machine for handling receive packet
	 */
	switch (handle_rx_state) {
	case TAB_STATE_RECV_RX_REQ:
		if (state_query_req->empty())
			break;

		gbn_query_req = state_query_req->read();
		rx_slot_id = gbn_query_req.gbn_header(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);

		recv_seqnum = gbn_query_req.gbn_header(
			SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		/*
		 * cook udp header from session ID,
		 * src ip and dest ip is needed in our application,
		 * port is not used
		 */
		rsp_udp_info.dest_ip = gbn_query_req.src_ip;
		rsp_udp_info.src_ip = gbn_query_req.dest_ip;
		rsp_udp_info.src_port = LEGOMEM_PORT;
		rsp_udp_info.dest_port = LEGOMEM_PORT;
		rsp_udp_info.length = 8;

		/*
		 * in response payload, src slot id and dest slot id is exchanged
		 */
		rsp_pkt.data(SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
			     SRC_SLOT_OFFSET) =
			gbn_query_req.gbn_header(DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
						 DEST_SLOT_OFFSET);
		rsp_pkt.data(DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
			     DEST_SLOT_OFFSET) =
			gbn_query_req.gbn_header(SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
						 SRC_SLOT_OFFSET);
		rsp_pkt.last = 1;
		rsp_pkt.keep = 0xff;

		if (gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
		    pkt_type_data) {
			expt_seqnum = expected_seqnum_array[rx_slot_id];
			handle_rx_state = TAB_STATE_HANDLE_DATA;
		} else if (gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
				   pkt_type_ack ||
			   gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
				   pkt_type_nack) {
			rx_last_ackd_seqnum = last_ackd_seqnum_array[rx_slot_id];
			rx_last_sent_seqnum = last_sent_seqnum_array[rx_slot_id];

			handle_rx_state = TAB_STATE_HANDLE_ACK;
		} else if (gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
				   pkt_type_syn ||
			   gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
				   pkt_type_fin) {
			state_query_rsp->write(true);
		} else {
			/* unknown packet type */
			state_query_rsp->write(false);
		}
		break;
	case TAB_STATE_HANDLE_DATA:
		/*
		 * if received data packet, generate ack/nack response
		 */
		if (recv_seqnum == expt_seqnum) {
			expected_seqnum_array[rx_slot_id] =
				expt_seqnum + 1;
			ack_enable_bitmap[rx_slot_id] = 1;
			deliver_data = true;
			/* deliever udp header */
			rsp_header->write(rsp_udp_info);
			/* generate response packet */
			rsp_pkt.data(PKT_TYPE_WIDTH - 1, 0) =
				pkt_type_ack;
			rsp_pkt.data(SEQ_OFFSET + SEQ_WIDTH - 1,
					SEQ_OFFSET) = expt_seqnum;

			rsp_payload->write(rsp_pkt);
		} else if (ack_enable_bitmap[rx_slot_id] == 1) {
			if (recv_seqnum > expt_seqnum) {
				deliver_data = false;
				/* disable ack */
				ack_enable_bitmap[rx_slot_id] = 0;
				/* deliever udp header */
				rsp_header->write(rsp_udp_info);
				/* generate response packet */
				rsp_pkt.data(PKT_TYPE_WIDTH - 1, 0) =
					pkt_type_nack;
				rsp_pkt.data(SEQ_OFFSET + SEQ_WIDTH - 1,
					     SEQ_OFFSET) = expt_seqnum - 1;

				rsp_payload->write(rsp_pkt);
			} else {
				/* received seqnum < expected seqnum */
				deliver_data = false;
				/* deliever udp header */
				rsp_header->write(rsp_udp_info);
				/* generate response packet */
				rsp_pkt.data(PKT_TYPE_WIDTH - 1, 0) =
					pkt_type_ack;
				rsp_pkt.data(SEQ_OFFSET + SEQ_WIDTH - 1,
						SEQ_OFFSET) = expt_seqnum - 1;

				rsp_payload->write(rsp_pkt);
			}
		}
		state_query_rsp->write(deliver_data);
		handle_rx_state = TAB_STATE_RECV_RX_REQ;
		break;
	case TAB_STATE_HANDLE_ACK:
		/*
		 * remove acked packets from unacked packets queue (ack)
		 * or retransmit unacked packets (nack)
		 */

		/* ACK packet not delivered to downstream */
		state_query_rsp->write(false);

		if (recv_seqnum > rx_last_ackd_seqnum) {
			last_ackd_seqnum_array[rx_slot_id] = recv_seqnum;
			PR("slot %d: [last ack %ld, last sent %ld]\n",
			   rx_slot_id.to_uint(), recv_seqnum.to_uint(),
			   rx_last_sent_seqnum.to_uint());

			if (recv_seqnum == rx_last_sent_seqnum) {
				/* disable timeout for this flow */
				rst_timer_req.rst_type = timer_rst_type_stop;
				rst_timer_req.slotid = gbn_query_req.gbn_header(
					DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
					DEST_SLOT_OFFSET);
				timer_rst_req->write(rst_timer_req);
				break;
			}

			if (gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
			    pkt_type_ack) {
				/* set net timeout for this flow and enable timeout */
				rst_timer_req.slotid = gbn_query_req.gbn_header(
					DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
					DEST_SLOT_OFFSET);
				rst_timer_req.rst_type = timer_rst_type_reset;
				timer_rst_req->write(rst_timer_req);
			} else {
				/*
				* if nack received, retransmit. restart
				* timer after finishing retransmission
				* retrans (recv_seqnum, last_sent_seqnum] for flow[i]
				*/
				retrans_data = true;
			}
		} else if (recv_seqnum == rx_last_ackd_seqnum &&
			   gbn_query_req.gbn_header(PKT_TYPE_WIDTH - 1, 0) ==
				   pkt_type_nack) {
			retrans_data = true;
		}

		if (retrans_data) {
			gbn_rt_req.slotid = gbn_query_req.gbn_header(
				DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
				DEST_SLOT_OFFSET);
			/* set retrans range */
			gbn_rt_req.seq_start = recv_seqnum + 1;
			gbn_rt_req.seq_end = rx_last_sent_seqnum;
			gbn_retrans_req->write(gbn_rt_req);
			retrans_data = false;
		}

		handle_rx_state = TAB_STATE_RECV_RX_REQ;
		break;
	default:
		break;
	}

	static ap_uint<SEQ_WIDTH> tx_last_ackd_seqnum, tx_last_sent_seqnum;
	ap_uint<SEQ_WIDTH> tx_ck_rsp;
	bool tx_complete;

	/*
	 * state machine for handling send packet
	 */
	switch (handle_tx_state) {
	case TAB_STATE_RECV_TX_REQ:
		if (check_full_req->empty())
			break;
		tx_slot_id = check_full_req->read(); 
		tx_last_sent_seqnum = last_sent_seqnum_array[tx_slot_id];
		handle_tx_state = TAB_STATE_REPL_TX_REQ;
		break;
	case TAB_STATE_REPL_TX_REQ:
		tx_last_ackd_seqnum = last_ackd_seqnum_array[tx_slot_id];
		if (tx_last_ackd_seqnum + WINDOW_SIZE > tx_last_sent_seqnum) {
			tx_ck_rsp = tx_last_sent_seqnum + 1;
			check_full_rsp->write(tx_ck_rsp);
			handle_tx_state = TAB_STATE_UPD_TX_INFO;		
		}
		break;
	case TAB_STATE_UPD_TX_INFO:
		if (tx_finish_sig->empty())
			break;
		tx_complete = tx_finish_sig->read();
		PR("slot %d finish send a packet\n", tx_slot_id.to_uint());

		if (tx_last_ackd_seqnum == tx_last_sent_seqnum) {
			rst_timer_req.slotid = tx_slot_id;
			rst_timer_req.rst_type = timer_rst_type_reset;
			timer_rst_req->write(rst_timer_req);
		}
		last_sent_seqnum_array[tx_slot_id] = tx_last_sent_seqnum + 1;
		handle_tx_state = TAB_STATE_RECV_TX_REQ;
		break;
	default:
		break;
	}

	ap_uint<SEQ_WIDTH> rt_last_acks_seqnum, rt_last_sent_seqnum;
	if (!rt_timeout_sig->empty()) {
		rt_slot_id = rt_timeout_sig->read();
		rt_last_acks_seqnum = last_sent_seqnum_array[rt_slot_id];
		rt_last_sent_seqnum = last_sent_seqnum_array[rt_slot_id];
		gbn_rt_req.slotid = rt_slot_id;
		gbn_rt_req.seq_start = rt_last_acks_seqnum + 1;
		gbn_rt_req.seq_end = rt_last_sent_seqnum;
		gbn_retrans_req->write(gbn_rt_req);
	}

	ap_uint<SLOT_ID_WIDTH> init_slot_id;
	if (!init_req->empty()) {
		/*
		 * initialize per-flow states when connection is first setup
		 */
		init_slot_id = init_req->read();
		PR("initialize slot %d\n", init_slot_id.to_uint());
		ack_enable_bitmap[init_slot_id] = 1;
		last_ackd_seqnum_array[init_slot_id] = 0;
		last_sent_seqnum_array[init_slot_id] = 0;
		expected_seqnum_array[init_slot_id] = 1;
	}
}
