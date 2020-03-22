#ifndef _RELNET_UNACKED_BUFFER_H_
#define _RELNET_UNACKED_BUFFER_H_

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <fpga/rel_net.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

void unacked_buffer(stream<struct timer_req>	*timer_rst_req,
		    stream<struct net_axis_64>	*tx_buff_payload,
		    stream<struct route_info>	*tx_buff_route_info,
		    stream<struct retrans_req>	*gbn_retrans_req);

#endif
