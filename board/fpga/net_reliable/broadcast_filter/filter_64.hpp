#ifndef _RELNET_FILTER64_H_
#define _RELNET_FILTER64_H_

#include <hls_stream.h>
#include <fpga/axis_net.h>
#include <fpga/axis_internal.h>
#include <fpga/kernel.h>

using hls::stream;

void filter_64(stream<struct udp_info>		*rx_header,
	       stream<struct net_axis_64>	*rx_payload,
	       stream<struct udp_info>		*usr_rx_header,
	       stream<struct net_axis_64>	*usr_rx_payload);

#endif
