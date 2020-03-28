#ifndef _RELNET_STATE_TAB64_H_
#define _RELNET_STATE_TAB64_H_

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <uapi/gbn.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

void state_table_64(stream<struct udp_info>		*rsp_header,
		    stream<struct net_axis_64>		*rsp_payload,
		    stream<struct query_req>		*state_query_req,
		    stream<bool>			*state_query_rsp,
		    stream<struct retrans_req>		*gbn_retrans_req,
		    stream<struct timer_req>		*timer_rst_req,
		    stream<ap_uint<SLOT_ID_WIDTH> >	*rt_timeout_sig,
		    stream<bool>			*tx_finish_sig,
		    stream<ap_uint<SLOT_ID_WIDTH> >	*check_full_req,
		    stream<ap_uint<SEQ_WIDTH> >		*check_full_rsp,
		    stream<ap_uint<SLOT_ID_WIDTH> >	*init_req);

#endif
