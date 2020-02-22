/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR

#include "tx_64.hpp"

enum udp_enqueue_status {
	udp_enqueue_wait,
	udp_enqueue_head,
	udp_enqueue_payload_1,
	udp_enqueue_payload_2
};

enum udp_dequeue_status {
	udp_dequeue_idle,
	udp_dequeue_ack,
	udp_retrans_head,
	udp_retrans_payload_1,
	udp_retrans_payload_2
};

/**
 * @tx_header: udp header sent to network stack
 * @tx_payload: udp payload sent to network stack
 * @usr_tx_header: received udp header from onboard pipeline
 * @usr_tx_payload: received udp payload from onboard pipeline
 * @ack_header: udp header of ack/nack from receiver(may not need)
 * @ack_payload: udp payload of ack/nack from receiver
 * @queue_rd_cmd: read queue command sent to queue
 * @queue_wr_cmd: write queue command sent to queue
 * @queue_wr_data: payload data sent to queue
 * @queue_rd_data: payload data read from queue
 * @rt_header: retransmit udp header
 * @rt_payload: retransmit udp payload
 */
void tx_64(stream<struct udp_info>	*tx_header,
	   stream<struct net_axis_64>	*tx_payload,
	   stream<struct udp_info>	*usr_tx_header,
	   stream<struct net_axis_64>	*usr_tx_payload,
	   stream<struct udp_info>	*ack_header,
	   stream<struct net_axis_64>	*ack_payload,
	   stream<struct bram_cmd>	*queue_rd_cmd,
	   stream<struct bram_cmd>	*queue_wr_cmd,
	   stream<struct net_axis_64>	*queue_wr_data,
	   stream<struct net_axis_64>	*queue_rd_data,
	   stream<struct udp_info>	*rt_header,
	   stream<struct net_axis_64>	*rt_payload
#ifdef DEBUG_MODE
	   ,ap_uint<1>			reset_seq
#endif
	)
{
#pragma HLS INTERFACE axis both port=tx_header
#pragma HLS INTERFACE axis both port=tx_payload
#pragma HLS INTERFACE axis both port=usr_tx_header
#pragma HLS INTERFACE axis both port=usr_tx_payload
#pragma HLS INTERFACE axis both port=ack_header
#pragma HLS INTERFACE axis both port=ack_payload
#pragma HLS INTERFACE axis both port=queue_rd_cmd
#pragma HLS INTERFACE axis both port=queue_wr_cmd
#pragma HLS INTERFACE axis both port=queue_wr_data
#pragma HLS INTERFACE axis both port=queue_rd_data
#pragma HLS INTERFACE axis both port=rt_header
#pragma HLS INTERFACE axis both port=rt_payload

#pragma HLS DATA_PACK variable=tx_header
#pragma HLS DATA_PACK variable=usr_tx_header
#pragma HLS DATA_PACK variable=ack_header
#pragma HLS DATA_PACK variable=queue_rd_cmd
#pragma HLS DATA_PACK variable=queue_wr_cmd
#pragma HLS DATA_PACK variable=rt_header

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum udp_enqueue_status enqueue_state = udp_enqueue_wait;
	static enum udp_dequeue_status dequeue_state = udp_dequeue_idle;

	/**
	 * seqnum info
	 *
	 * 		 |<-----window----->|
	 * ++++++++++++++|----------********|
	 * 		^	   ^
	 * 	last_ackd_seqnum   |
	 * 		    last_sent_seqnum
	 */
	static ap_uint<SEQ_WIDTH> last_ackd_seqnum = 0;
	static ap_uint<SEQ_WIDTH> last_sent_seqnum = 0;
	static bool empty;
	empty = last_sent_seqnum == last_ackd_seqnum;

	// header queue info
	static struct udp_info unackd_header_queue[WINDOW_SIZE];
#pragma HLS dependence variable=unackd_header_queue intra false
#pragma HLS dependence variable=unackd_header_queue inter false
#pragma HLS ARRAY_PARTITION variable=unackd_header_queue
#pragma HLS DATA_PACK variable=unackd_header_queue

	static ap_uint<8> head = 0;
	static ap_uint<8> rear = 0;

	// timer info
	static bool retrans = false;
	static bool timer_rst = false;
	static long timer = -1;  // timer idle

	if (timer > 0) timer--;
	if (timer_rst) {
		timer = RETRANS_TIMEOUT_CYCLE;
		timer_rst = false;
	}
	if (timer == 0) retrans = true;
	/**
	 * if no packet waited to acknowledge
	 * and no packet to send, disable timer
	 */
	if (empty) timer = -1;
	PR("timer: %lld\n", timer);

	static unsigned int pkt_size_cnt = 1;
	static unsigned int retrans_size_cnt = 0;

#ifdef DEBUG_MODE
	/**
	 * this reset is only used for hls software test,
	 * in hardware we can use the blocklevel reset signal
	 */
	if (reset_seq) {
		last_ackd_seqnum = 0;
		last_sent_seqnum = 0;
		enqueue_state = udp_enqueue_wait;
		dequeue_state = udp_dequeue_idle;
		timer = -1;
		retrans = false;
		head = 0;
		rear = 0;
		pkt_size_cnt = 1;
		retrans_size_cnt = 0;
	}
#endif

	// enqueue state machine
	struct udp_info send_udp_info;
	struct net_axis_64 send_pkt;
	struct bram_cmd cmd_w;
	static bool start = false;

	switch (enqueue_state) {
	case udp_enqueue_wait:
		/**
		 * wait for available slot in the queue
		 */
		if (last_sent_seqnum < last_ackd_seqnum + WINDOW_SIZE - 1) {
			enqueue_state = udp_enqueue_head;
			start = empty;
		}
		break;
	case udp_enqueue_head:
		if (usr_tx_header->empty())
			break;
		send_udp_info = usr_tx_header->read();
		PR("get header from MMU: %x:%d -> %x:%d\n",
		   send_udp_info.src_ip.to_uint(),
		   send_udp_info.src_port.to_uint(),
		   send_udp_info.dest_ip.to_uint(),
		   send_udp_info.dest_port.to_uint());

		/**
		 * send udp header to tx port and unack'd queue
		 */
		unackd_header_queue[rear] = send_udp_info;
		tx_header->write(send_udp_info);
		enqueue_state = udp_enqueue_payload_1;
		last_sent_seqnum++;
		break;
	case udp_enqueue_payload_1:
		/**
		 * generate relnet header with sequence#
		 */
		enqueue_state = udp_enqueue_payload_2;
		send_pkt.data(7, 0) = pkt_type_data;
		send_pkt.data(7 + SEQ_WIDTH, 8) = last_sent_seqnum;
		send_pkt.keep = 0xff;
		send_pkt.last = 0;
		PR("send gbn header to net [type %d, seq %lld]\n",
		   send_pkt.data(7, 0).to_uint(),
		   send_pkt.data(7 + SEQ_WIDTH, 8).to_uint64());

		cmd_w.index = rear;
		cmd_w.offset = 0;
		queue_wr_cmd->write(cmd_w);
		queue_wr_data->write(send_pkt);
		tx_payload->write(send_pkt);
		break;
	case udp_enqueue_payload_2:
		/**
		 * send udp payload to tx port and unack'd queue
		 */
		if (usr_tx_payload->empty())
			break;
		send_pkt = usr_tx_payload->read();
		PR("get payload from MMU: %llx\n", send_pkt.data.to_uint64());
		cmd_w.index = rear;
		cmd_w.offset = pkt_size_cnt;
		pkt_size_cnt++;

		queue_wr_cmd->write(cmd_w);
		queue_wr_data->write(send_pkt);
		tx_payload->write(send_pkt);

		if (send_pkt.last == 1) {
			enqueue_state = udp_enqueue_wait;
			pkt_size_cnt = 1;
			rear = (rear + 1) & WINDOW_INDEX_MSK;
			timer_rst = start;
		}
		break;
	default:
		break;
	}

	// dequeue state machine
	static struct net_axis_64 ack_pkt;
	static struct udp_info ack_udp_info;
	ap_uint<SEQ_WIDTH> recv_seqnum;

	static unsigned char retrans_i, retrans_end;
	struct udp_info retrans_udp_info;
	struct net_axis_64 retrans_pkt;
	struct bram_cmd cmd_r;

	switch (dequeue_state) {
	case udp_dequeue_idle:
		if (!ack_header->empty() && !ack_payload->empty()) {
			ack_udp_info = ack_header->read();
			ack_pkt = ack_payload->read();
			PR("receive ack udp header from rx: %x:%d -> %x:%d\n",
			   ack_udp_info.src_ip.to_uint(),
			   ack_udp_info.src_port.to_uint(),
			   ack_udp_info.dest_ip.to_uint(),
			   ack_udp_info.dest_port.to_uint());
			PR("receive ack gbn header: [type %d, seq %lld]\n",
			   ack_pkt.data(7, 0).to_uint(),
			   ack_pkt.data(7 + SEQ_WIDTH, 8).to_uint64());
			dequeue_state = udp_dequeue_ack;
			break;
		}
		if (retrans) {
			dequeue_state = udp_retrans_head;
			retrans_i = head;
			retrans_end = rear;
			break;
		}
		break;
	case udp_dequeue_ack:
		/**
		 * remove acked packets from unacked packets queue (ack)
		 * or retransmit unacked packets (nack)
		 */
		recv_seqnum = ack_pkt.data(7 + SEQ_WIDTH, 8);

		if (recv_seqnum > last_ackd_seqnum) {
			/* move head forward */
			head = (head + (recv_seqnum - last_ackd_seqnum)) &
			       WINDOW_INDEX_MSK;
			// TODO: what if seqnum overflows?
			last_ackd_seqnum = recv_seqnum;
			timer_rst = true;
		}
		if (ack_pkt.data(7, 0) == pkt_type_nack) {
			/**
			 * if nack received, retransmit. restart
			 * timer after finishing retransmission
			 */
			retrans = true;
			/* set retrans range */
			retrans_i = head;
			retrans_end = rear;
			timer = -1;
			dequeue_state = udp_retrans_head;
			break;
		}
		PR("recv seq#: %d, last acked seq#: %d, head: %d, rear: %d\n",
		   recv_seqnum.to_uint(), last_ackd_seqnum.to_uint(),
		   head.to_uchar(), rear.to_uchar());
		dequeue_state = udp_dequeue_idle;
		break;
	case udp_retrans_head:
		if (retrans_i == retrans_end) {
			/* retransmission finish, restart timer */
			dequeue_state = udp_dequeue_idle;
			retrans = false;
			timer_rst = true;
			break;
		}

		retrans_udp_info = unackd_header_queue[retrans_i];
		rt_header->write(retrans_udp_info);
		PR("retrans udp header: %x:%d -> %x:%d\n",
		   retrans_udp_info.src_ip.to_uint(),
		   retrans_udp_info.src_port.to_uint(),
		   retrans_udp_info.dest_ip.to_uint(),
		   retrans_udp_info.dest_port.to_uint());
		dequeue_state = udp_retrans_payload_1;
		break;
	case udp_retrans_payload_1:
		/* request payload from queue */
		cmd_r.index = retrans_i;
		cmd_r.offset = retrans_size_cnt;
		retrans_size_cnt++;

		queue_rd_cmd->write(cmd_r);
		dequeue_state = udp_retrans_payload_2;
		break;
	case udp_retrans_payload_2:
		/* retrans payload */
		if (queue_rd_data->empty())
			break;
		retrans_pkt = queue_rd_data->read();
		rt_payload->write(retrans_pkt);
		dequeue_state = udp_retrans_payload_1;
		PR("retrans payload: %llx, if gbn header [type %d, seq %lld]\n",
		   retrans_pkt.data.to_uint64(),
		   retrans_pkt.data(7, 0).to_uint(),
		   retrans_pkt.data(7 + SEQ_WIDTH, 8).to_uint64());

		if (retrans_pkt.last == 1) {
			dequeue_state = udp_retrans_head;
			retrans_i = (retrans_i + 1) & WINDOW_INDEX_MSK;
			retrans_size_cnt = 0;
		}
		break;
	default:
		break;
	}
}
