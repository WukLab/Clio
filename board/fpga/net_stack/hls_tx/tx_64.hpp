#ifndef _RELNET_TX64_H_
#define _RELNET_TX64_H_

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <fpga/rel_net.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

void tx_64(stream<struct udp_info>		*tx_header,
	   stream<struct net_axis_64>		*tx_payload,
	   stream<struct udp_info>		*usr_tx_header,
	   stream<struct net_axis_64>		*usr_tx_payload,
	   stream<ap_uint<SLOT_ID_WIDTH> >	*check_full_req,
	   stream<ap_uint<SEQ_WIDTH> >		*check_full_rsp,
	   stream<bool>				*tx_finish_sig,
	   stream<struct net_axis_64>		*tx_buff_payload,
	   stream<struct route_info>		*tx_buff_route_info);

#endif
