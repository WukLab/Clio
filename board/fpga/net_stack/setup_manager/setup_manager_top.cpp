#include "setup_manager.hpp"

/*
 * @init_req: session open/close request from SoC
 * @conn_set_req: session states setup request to state table
 * @timer_rst_req: timer set request to timer
 * 
 * setup_manager is the gate between GBN and SoC:
 * it accepts control requests from SoC, such as open/close session.
 *
 * Upon receiving a open request, we will send a request to state_table via @init_req
 * to 1) reset seqnum, 2) set it valid. We will also send request to timer to setup.
 *
 * Upon receiving a close request, we will send a request to state_table via @init_req
 * to set it invalid. We will also send request to timer to disable it.
 */
void setup_manager(stream<struct conn_mgmt_req>		*init_req,
		   stream<struct conn_mgmt_req>		*conn_set_req,
		   stream<struct timer_req>		*timer_rst_req)
{
#pragma HLS INTERFACE axis both port=init_req
#pragma HLS INTERFACE axis both port=conn_set_req
#pragma HLS INTERFACE axis both port=timer_rst_req

#pragma HLS DATA_PACK variable=conn_set_req
#pragma HLS DATA_PACK variable=timer_rst_req
#pragma HLS DATA_PACK variable=init_req

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS PIPELINE

	struct conn_mgmt_req conn_req;
	struct timer_req rst_timer_req;

	if (!conn_set_req->empty()) {
		conn_req = conn_set_req->read();
		if (conn_req.set_type == set_type_open ||
		    conn_req.set_type == set_type_close) {
			/* forward the request to state table */
			init_req->write(conn_req);
			/* at both open and close session, deactive timeout */
			rst_timer_req.rst_type = timer_rst_type_stop;
			rst_timer_req.slotid = conn_req.slotid;
			timer_rst_req->write(rst_timer_req);
		}
	}
}
