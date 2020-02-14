#ifndef _RELNET_ARBITER64_H_
#define _RELNET_ARBITER64_H_

#include <fpga/rel_net.h>
#include <fpga/axis_net.h>
#include <hls_stream.h>
#include <fpga/kernel.h>
#include <uapi/net_header.h>

using namespace hls;

/**
 * @rsp_header: udp header for ack/nack response
 * @rsp_payload: udp payload for ack/nack response
 * @tx_header: udp header sent to network stack
 * @tx_payload: udp payload sent to network stack
 * @rt_header: retransmit udp header
 * @rt_payload: retransmit udp payload
 * @out_header: final header interface to network stack
 * @out_payload: final payload interface to network stack
 * priority:
 * 1. rsp
 * 2. retrans
 * 3. normal trans
 */
void arbiter_64(stream<struct udp_info>		*rsp_header,
		stream<struct net_axis_64>	*rsp_payload,
		stream<struct udp_info>		*tx_header,
		stream<struct net_axis_64>	*tx_payload,
		stream<struct udp_info>		*rt_header,
		stream<struct net_axis_64>	*rt_payload,
		stream<struct udp_info>		*out_header,
		stream<struct net_axis_64>	*out_payload);

#endif
