/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define ENABLE_PR

#include "timer.hpp"

enum timeout_status {
	TIMEOUT_STATE_READ,
	TIMEOUT_STATE_CHEK
};

struct rt_timer_entry {
	bool		active;
	ap_uint<31>	time;
};

void retrans_timer(stream<struct timer_req>		*timer_rst_req,
		   stream<ap_uint<SLOT_ID_WIDTH> >	*rt_timeout_sig)
{
#pragma HLS INTERFACE axis both port=timer_rst_req
#pragma HLS INTERFACE axis both port=rt_timeout_sig
#pragma HLS DATA_PACK variable=timer_rst_req

#pragma HLS PIPELINE
#pragma HLS INTERFACE ap_ctrl_none port=return

	static rt_timer_entry timeout_array[MAX_NR_CONN] = {};
	struct rt_timer_entry current_entry;
#pragma HLS DATA_PACK variable=timeout_array
#pragma HLS DEPENDENCE variable=timeout_array inter false
#pragma HLS RESOURCE variable=timeout_array core=RAM_T2P_BRAM

	struct timer_req rst_req;
	ap_uint<SLOT_ID_WIDTH> rst_slot_id;
	static ap_uint<SLOT_ID_WIDTH> scan_slot_id = 0;

	if (!timer_rst_req->empty()) {
		rst_req = timer_rst_req->read();
		rst_slot_id = rst_req.slotid;
		if (rst_req.rst_type == timer_rst_type_reset) {
			timeout_array[rst_slot_id].time = RETRANS_TIMEOUT_CYCLE / MAX_NR_CONN;
			timeout_array[rst_slot_id].active = true;
		} else if (rst_req.rst_type == timer_rst_type_stop) {
			timeout_array[rst_slot_id].active = false;
		}
	} else {
		scan_slot_id++;
		PR("scan slot %d\n", scan_slot_id.to_ushort());
		current_entry = timeout_array[scan_slot_id];
		if (current_entry.active) {
			if (current_entry.time > 0) {
				current_entry.time--;
			} else {
				rt_timeout_sig->write(scan_slot_id);
				current_entry.active = false;
			}
		}
		timeout_array[scan_slot_id] = current_entry;
	}
}
