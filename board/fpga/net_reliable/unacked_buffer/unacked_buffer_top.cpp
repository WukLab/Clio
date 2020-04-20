#define ENABLE_PR

#include "unacked_buffer.hpp"

enum buff_recv_status {
	BUF_STATE_RECV_INFO,
	BUF_STATE_WRITE_HEADER,
	BUF_STATE_RECV_PAYLOAD
};

enum buff_retrans_status {
	BUF_STATE_RECV_REQ,
	BUF_STATE_READ_LEN,
	BUF_STATE_RETRANS_HEADER,
	BUF_STATE_RETRANS_PAYLOAD
};

void unacked_buffer(stream<struct timer_req>	*timer_rst_req,
		    stream<struct net_axis_64>	*tx_buff_payload,
		    stream<struct route_info>	*tx_buff_route_info,
		    stream<struct retrans_req>	*gbn_retrans_req,
		    stream<struct net_axis_64>	*rt_payload,
		    stream<struct udp_info>	*rt_header,
		    stream<struct dm_cmd>	*dm_rd_cmd_1,
		    stream<struct dm_cmd>	*dm_rd_cmd_2,
		    stream<struct net_axis_64>	*dm_rd_data,
		    stream<struct dm_cmd>	*dm_wr_cmd,
		    stream<struct net_axis_64>	*dm_wr_data)
{
#pragma HLS INTERFACE axis both port=timer_rst_req
#pragma HLS INTERFACE axis both port=tx_buff_payload
#pragma HLS INTERFACE axis both port=tx_buff_route_info
#pragma HLS INTERFACE axis both port=gbn_retrans_req
#pragma HLS INTERFACE axis both port=rt_payload
#pragma HLS INTERFACE axis both port=rt_header
#pragma HLS INTERFACE axis both port=dm_rd_cmd_1
#pragma HLS INTERFACE axis both port=dm_rd_cmd_2
#pragma HLS INTERFACE axis both port=dm_rd_data
#pragma HLS INTERFACE axis both port=dm_wr_cmd
#pragma HLS INTERFACE axis both port=dm_wr_data

#pragma HLS DATA_PACK variable=timer_rst_req
#pragma HLS DATA_PACK variable=tx_buff_route_info
#pragma HLS DATA_PACK variable=gbn_retrans_req
#pragma HLS DATA_PACK variable=rt_header
#pragma HLS DATA_PACK variable=dm_rd_cmd_1
#pragma HLS DATA_PACK variable=dm_rd_cmd_2
#pragma HLS DATA_PACK variable=dm_wr_cmd

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	static enum buff_recv_status recv_state = BUF_STATE_RECV_INFO;
	static enum buff_retrans_status retrans_state = BUF_STATE_RECV_REQ;

	/*
	 * We only need to dest ip here since the ip
	 * for each board is pre-determined, so we don't
	 * need to take care of it at GBN layer
	 */
	static ap_uint<32> dest_ip_array[NR_MAX_SESSIONS_PER_NODE];
#pragma HLS DEPENDENCE variable=dest_ip_array inter false

	static struct net_axis_64 buff_packet;
	static struct route_info buff_route_info;
	unsigned buff_slot_id;
	unsigned buff_window_idx;

	struct dm_cmd out_cmd;
	struct dm_cmd in_cmd;

	struct timer_req rst_timer_req;
	static struct retrans_req gbn_rt_req;

	switch (recv_state) {
	case BUF_STATE_RECV_INFO:
		if (!tx_buff_payload->empty() && !tx_buff_route_info->empty()) {
			buff_packet = tx_buff_payload->read();
			buff_route_info = tx_buff_route_info->read();

			buff_slot_id = buff_packet.data(
				SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1,
				SRC_SLOT_OFFSET);
			/* calculate the index in the window based on seqnum */ 
			buff_window_idx = buff_packet.data(
				SEQ_OFFSET + WINDOW_SIZE_WIDTH - 1, SEQ_OFFSET);
			PR("slot %d, window index %d\n", buff_slot_id,
			   buff_window_idx);

			dest_ip_array[buff_slot_id] = buff_route_info.dest_ip;

			struct net_axis_64 length;
			length.data = buff_route_info.length;
			length.keep = 0xff;
			length.last = 0;

			/*
			 * write packet length in the first 8 bytes
			 * total write length is packet length + 8
			 */
			out_cmd.btt = buff_route_info.length + 8;
			out_cmd.type = DM_CMD_TYPE_INCR;
			out_cmd.dsa = 0;
			out_cmd.eof = 1;
			out_cmd.drr = 0;
			out_cmd.start_address =
				BUFF_ADDRESS_START +
				((buff_slot_id << WINDOW_SIZE_WIDTH) +
				 buff_window_idx) * MAX_PACKET_SIZE;
			dm_wr_cmd->write(out_cmd);
			dm_wr_data->write(length);

			recv_state = BUF_STATE_WRITE_HEADER;
		}
		break;
	case BUF_STATE_WRITE_HEADER:
		/* write gbn header into dram */
		dm_wr_data->write(buff_packet);
		recv_state = BUF_STATE_RECV_PAYLOAD;
		break;
	case BUF_STATE_RECV_PAYLOAD:
		if (tx_buff_payload->empty())
			break;
		buff_packet = tx_buff_payload->read();
		
		dm_wr_data->write(buff_packet);
		if (buff_packet.last == 1) {
			recv_state = BUF_STATE_RECV_INFO;
		}
		break;
	default:
		break;
	}

	struct udp_info retrans_udp_info;
	struct net_axis_64 retrans_pkt, rd_length;
	struct ap_uint<32> rt_dest_ip;
	static unsigned rt_slot_id;
	static unsigned rt_seq;
	static unsigned slot_base, start_addr;

	switch (retrans_state) {
	case BUF_STATE_RECV_REQ:
		if (gbn_retrans_req->empty())
			break;
		gbn_rt_req = gbn_retrans_req->read();
		PR("receive retrans request: [from %ld to %ld, slot %d]\n",
		   gbn_rt_req.seq_start.to_uint(), gbn_rt_req.seq_end.to_uint(),
		   gbn_rt_req.slotid.to_uint());
		
		if (gbn_rt_req.seq_end < gbn_rt_req.seq_start)
			break;

		rt_slot_id = gbn_rt_req.slotid;
		rt_seq = gbn_rt_req.seq_start;
		/* calculate the start address for that slot */
		slot_base = BUFF_ADDRESS_START +
			    (rt_slot_id << WINDOW_SIZE_WIDTH) * MAX_PACKET_SIZE;
		PR("slot %d base address: %lx\n", rt_slot_id, slot_base);

		retrans_state = BUF_STATE_READ_LEN;
		break;
	case BUF_STATE_READ_LEN:
		/* calculate the start address for that packet */
		start_addr =
			slot_base + (rt_seq & WINDOW_IDX_MSK) * MAX_PACKET_SIZE;

		in_cmd.btt = 8;
		in_cmd.type = DM_CMD_TYPE_INCR;
		in_cmd.dsa = 0;
		in_cmd.eof = 1;
		in_cmd.drr = 0;
		in_cmd.rsvd = 0;
		in_cmd.start_address = start_addr;

		/* read the packet length of packet #rt_seq */
		dm_rd_cmd_1->write(in_cmd);
		retrans_state = BUF_STATE_RETRANS_HEADER;
	case BUF_STATE_RETRANS_HEADER:
		if (dm_rd_data->empty())
			break;

		rd_length = dm_rd_data->read();

		in_cmd.btt = rd_length.data(15, 0);
		in_cmd.type = DM_CMD_TYPE_INCR;
		in_cmd.dsa = 0;
		in_cmd.eof = 1;
		in_cmd.drr = 0;
		in_cmd.rsvd = 0;
		in_cmd.start_address = start_addr + 8;

		rt_dest_ip = dest_ip_array[rt_slot_id];

		retrans_udp_info.dest_ip = rt_dest_ip;
		retrans_udp_info.src_ip = 0;
		retrans_udp_info.dest_port = LEGOMEM_PORT;
		retrans_udp_info.src_port = LEGOMEM_PORT;
		retrans_udp_info.length = rd_length.data(15, 0);

		/* read the packet content of packet #rt_seq */
		dm_rd_cmd_2->write(in_cmd);
		rt_header->write(retrans_udp_info);

		retrans_state = BUF_STATE_RETRANS_PAYLOAD;
		break;
	case BUF_STATE_RETRANS_PAYLOAD:
		if (dm_rd_data->empty())
			break;

		retrans_pkt = dm_rd_data->read();
		rt_payload->write(retrans_pkt);

		if (retrans_pkt.last == 1) {
			rt_seq++;
			if (rt_seq > gbn_rt_req.seq_end) {
				/* retrans finish, reset timeout */
				rst_timer_req.rst_type = timer_rst_type_reset;
				rst_timer_req.slotid = rt_slot_id;
				timer_rst_req->write(rst_timer_req);
				retrans_state = BUF_STATE_RECV_REQ;
			} else {
				/* retrans next packet */
				retrans_state = BUF_STATE_READ_LEN;
			}
		}
		break;
	default:
		break;
	}
}
