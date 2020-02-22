#ifndef _RELNET_QUEUE64_H_
#define _RELNET_QUEUE64_H_

#include <fpga/rel_net.h>
#include <fpga/axis_net.h>
#include <hls_stream.h>
#include <fpga/kernel.h>
#include <uapi/net_header.h>

using hls::stream;

void queue_64(stream<struct bram_cmd>		*rd_cmd,
	      stream<struct bram_cmd>		*wr_cmd,
	      stream<struct net_axis_64>	*rd_data,
	      stream<struct net_axis_64>	*wr_data);

#endif
