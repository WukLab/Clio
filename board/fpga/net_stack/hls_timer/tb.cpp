/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */
#define RETRANS_TIMEOUT_CYCLE 2

#include "timer.hpp"

int main()
{
	stream<struct timer_req> timer_rst_req;
	stream<ap_uint<SLOT_ID_WIDTH> > rt_timeout_sig;

	struct timer_req set_req;
	set_req.rst_type = timer_rst_type_reset;
	set_req.slotid = 42;
	timer_rst_req.write(set_req);

	for (int i = 0; i < (NR_MAX_SESSIONS_PER_NODE * (RETRANS_TIMEOUT_CYCLE + 1)); i++) {
		retrans_timer(&timer_rst_req, &rt_timeout_sig);

		if (!rt_timeout_sig.empty()) {
			ap_uint<SLOT_ID_WIDTH> slotid = rt_timeout_sig.read();
			dph("slot %d timeout\n", slotid.to_ushort());
		}
	}
}
