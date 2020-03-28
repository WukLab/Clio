#ifndef _RELNET_SETUP_MANAGER_H_
#define _RELNET_SETUP_MANAGER_H_

#include <fpga/axis_net.h>
#include <fpga/kernel.h>
#include <uapi/gbn.h>
#include <fpga/axis_internal.h>
#include <hls_stream.h>

using hls::stream;

void setup_manager(stream<ap_uint<SLOT_ID_WIDTH> >	*init_req,
		   stream<struct conn_mgmt_req>		*conn_set_req,
		   stream<struct timer_req>		*timer_rst_req);

#endif
