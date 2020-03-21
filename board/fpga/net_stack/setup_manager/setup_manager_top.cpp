#include "setup_manager.hpp"

void setup_manager(stream<ap_uint<SLOT_ID_WIDTH> >	*init_req,
		   stream<struct conn_mgmt_req>		*conn_set_req,
		   stream<struct timer_req>		*timer_rst_req)
{
#pragma HLS INTERFACE axis both port=init_req
#pragma HLS INTERFACE axis both port=conn_set_req
#pragma HLS INTERFACE axis both port=timer_rst_req

#pragma HLS DATA_PACK variable=conn_set_req
#pragma HLS DATA_PACK variable=timer_rst_req

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	struct conn_mgmt_req conn_req;
	struct timer_req rst_timer_req;
	ap_uint<SLOT_ID_WIDTH> slot_id;

	if (!conn_set_req->empty()) {
		conn_req = conn_set_req->read();
		if (conn_req.set_type == set_type_setup) {
			slot_id = conn_req.slotid;
			init_req->write(slot_id);
		} else if (conn_req.set_type == set_type_close) {
			slot_id = conn_req.slotid;
			rst_timer_req.rst_type = timer_rst_type_stop;
			rst_timer_req.slotid = slot_id;
			timer_rst_req->write(rst_timer_req);
		}
	}
}
