#ifndef _RELNET_TIMER_H_
#define _RELNET_TIMER_H_

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <fpga/rel_net.h>
#include <hls_stream.h>
#include "../state_table/statetable_64.hpp"

using hls::stream;

void retrans_timer(stream<struct timer_req>		*timer_rst_req,
		   stream<ap_uint<SLOT_ID_WIDTH> >	*rt_timeout_sig);

#endif
