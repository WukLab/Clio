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

	static rt_timer_entry timeout_array[NR_MAX_SESSIONS_PER_NODE] = {};
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
		current_entry = timeout_array[rst_slot_id];
		/* reset or stop the timer based on request type */
		if (rst_req.rst_type == timer_rst_type_reset) {
			current_entry.time = RETRANS_TIMEOUT_CYCLE / NR_MAX_SESSIONS_PER_NODE;
			current_entry.active = true;
		} else if (rst_req.rst_type == timer_rst_type_stop) {
			current_entry.active = false;
		}
		timeout_array[rst_slot_id] = current_entry;
	} else {
		/*
		 * Since the number of maximum connections is big, we can't
		 * count the timeout for all connections simutaneously.
		 * Thus, we keep scanning the timeout table. Every timer get
		 * a check and decrease every NR_MAX_SESSIONS_PER_NODE cycles.
		 */
		scan_slot_id++;
		PR("scan slot %d\n", scan_slot_id.to_ushort());
		current_entry = timeout_array[scan_slot_id];
		/* only check when this slot is active */
		if (current_entry.active) {
			if (current_entry.time > 0) {
				current_entry.time--;
			} else {
				/*
				 * If time decrease to 0, fire timeout signal
				 * and deactive this slot
				 */ 
				rt_timeout_sig->write(scan_slot_id);
				current_entry.active = false;
			}
		}
		timeout_array[scan_slot_id] = current_entry;
	}
}
