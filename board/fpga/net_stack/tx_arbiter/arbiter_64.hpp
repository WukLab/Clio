#ifndef _RELNET_ARBITER64_H_
#define _RELNET_ARBITER64_H_

#include <uapi/gbn.h>
#include <fpga/axis_net.h>
#include <hls_stream.h>
#include <fpga/kernel.h>

using namespace hls;

void arbiter_64(stream<struct udp_info>		*rsp_header,
		stream<struct net_axis_64>	*rsp_payload,
		stream<struct udp_info>		*tx_header,
		stream<struct net_axis_64>	*tx_payload,
		stream<struct udp_info>		*rt_header,
		stream<struct net_axis_64>	*rt_payload,
		stream<struct udp_info>		*out_header,
		stream<struct net_axis_64>	*out_payload);

#endif
