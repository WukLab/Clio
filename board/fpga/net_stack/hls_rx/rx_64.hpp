#ifndef _RELNET_RX64_H_
#define _RELNET_RX64_H_

#include <hls_stream.h>
#include <fpga/rel_net.h>
#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <uapi/net_header.h>

using hls::stream;

/**
 * @rx_header: received udp header from network stack
 * @rx_payload: received udp payload from network stack
 * @rsp_header: udp header for ack/nack response
 * @rsp_payload: udp payload for ack/nack response
 * @ack_header: received ack/nack sent to unack'd queue
 * @ack_payload: received ack/nack sent to unack'd queue
 * @usr_rx_header: udp header sent to onboard pipeline
 * @usr_rx_payload: udp payload sent to onboard pipeline
 */
void rx_64(stream<struct udp_info>	*rx_header,
	   stream<struct net_axis_64>	*rx_payload,
	   stream<struct udp_info>	*rsp_header,
	   stream<struct net_axis_64>	*rsp_payload,
	   stream<struct udp_info>	*ack_header,
	   stream<struct net_axis_64>	*ack_payload,
	   stream<struct udp_info>	*usr_rx_header,
	   stream<struct net_axis_64>	*usr_rx_payload
#ifdef DEBUG_MODE
	   ,ap_uint<1>			reset_seq
#endif
);

#endif
