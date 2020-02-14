/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#define ENABLE_PR

#include <stdio.h>
#include "queue_64.hpp"
#include "../hls_tx/tx_64.hpp"

void queue_64(stream<struct bram_cmd>		*rd_cmd,
	      stream<struct bram_cmd>		*wr_cmd,
	      stream<struct net_axis_64>	*rd_data,
	      stream<struct net_axis_64>	*wr_data)
{
#pragma HLS INTERFACE axis both port=rd_cmd
#pragma HLS INTERFACE axis both port=wr_cmd
#pragma HLS INTERFACE axis both port=rd_data
#pragma HLS INTERFACE axis both port=wr_data

#pragma HLS DATA_PACK variable=rd_cmd
#pragma HLS DATA_PACK variable=wr_cmd

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

/**
 * ! here we assume there is no intra or inter cycle dependency
 * ! since I always enqueue from the rear while dequeue from the head
 * ! but I can not be very sure and further test need to be done
 */
	static struct net_axis_64 unackd_payload_queue[WINDOW_SIZE][MAX_PACKET_SIZE];
#pragma HLS dependence variable=unackd_payload_queue intra false
#pragma HLS dependence variable=unackd_payload_queue inter false
//#pragma HLS ARRAY_PARTITION variable=unackd_payload_queue dim=1
#pragma HLS DATA_PACK variable=unackd_payload_queue

	struct bram_cmd cmd_r, cmd_w;
	struct net_axis_64 rd_pkt, wr_pkt;

	unsigned char index;
	unsigned short offset;

	if (!rd_cmd->empty()) {
		cmd_r = rd_cmd->read();
		index = cmd_r.index;
		offset = cmd_r.offset;
		rd_pkt = unackd_payload_queue[index][offset];
		rd_data->write(rd_pkt);
		PR("read from [%d][%d]: %llx\n", index, offset,
		   rd_pkt.data.to_uint64());
	}

	if (!wr_cmd->empty() && !wr_data->empty()) {
		cmd_w = wr_cmd->read();
		wr_pkt = wr_data->read();
		index = cmd_w.index;
		offset = cmd_w.offset;
		unackd_payload_queue[index][offset] = wr_pkt;
		PR("write to [%d][%d]: %llx\n", index, offset,
		   wr_pkt.data.to_uint64());
	}
}
