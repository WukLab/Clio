#ifndef _RELNET_TX64_H_
#define _RELNET_TX64_H_

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <fpga/rel_net.h>
#include <hls_stream.h>
#include <uapi/net_header.h>

using hls::stream;

struct bram_cmd {
	unsigned char		index;
	unsigned short		offset;
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
);

#endif
