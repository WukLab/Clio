#ifndef _RELNET_UNACKED_BUFFER_H_
#define _RELNET_UNACKED_BUFFER_H_

#define AP_INT_MAX_W	(2048)

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <uapi/gbn.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

/*
 * This is the command word sent over to Datamover.
 * Defined at PG022 Figure 2-1 Command Word Layout.
 * NOTE: when you configure the Datamover, make btt 23 bits.
 */
#define DM_CMD_TYPE_FIXED	0
#define DM_CMD_TYPE_INCR	1
struct dm_cmd {
	ap_uint<23>	btt;
	ap_uint<1>	type;
	ap_uint<6>	dsa;
	ap_uint<1>	eof;
	ap_uint<1>	drr;
	ap_uint<32>	start_address;
	ap_uint<4>	tag;
	ap_uint<4>	rsvd;
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
		    stream<struct net_axis_64>	*dm_wr_data);

#endif
