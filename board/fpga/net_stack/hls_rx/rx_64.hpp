#ifndef _RELNET_RX64_H_
#define _RELNET_RX64_H_

#include <hls_stream.h>
#include <uapi/gbn.h>
#include <fpga/axis_net.h>
#include <fpga/axis_internal.h>
#include <fpga/kernel.h>

using hls::stream;

void rx_64(stream<struct udp_info>	*rx_header,
	   stream<struct net_axis_64>	*rx_payload,
	   stream<struct query_req>	*state_query_req,
	   stream<bool>			*state_query_rsp,
	   stream<struct udp_info>	*usr_rx_header,
	   stream<struct net_axis_64>	*usr_rx_payload);

#endif
