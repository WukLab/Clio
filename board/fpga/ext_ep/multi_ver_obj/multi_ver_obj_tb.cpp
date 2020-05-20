/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <iostream>
#include <algorithm>
#include "multi_ver_obj_tb.h"

#define MEM_SIZE		(1UL << 30)

using namespace std;
using namespace hls;

bool delay_cycles(bool activate, int target)
{
	static int counter = 0;

	if (activate)
		counter++;

	if (counter < target)
		return false;
	else {
		counter = 0;
		return true;
	}
}

void whole_design_loop(const int num_client, const int object_count, const int soc_datapath_latency,
		const int soc_ctrlpath_latency, const int mem_datapath_latency, const int tb_slowdowm)
{
	// simulated objects
	SocSim soc(MEM_SIZE, soc_datapath_latency, soc_ctrlpath_latency);
	MemSim mem(MEM_SIZE, mem_datapath_latency);
	NetSim net(object_count, num_client);
	XbarSim xbar;

	// fifos, naming helps cosim
	stream<ap_uint<DATAWIDTH> > net2xbar("net2xbar"), xbar2net("xbar2net");
	stream<ap_uint<DATAWIDTH> > mem2xbar("mem2xbar"), xbar2mem("xbar2mem");
	stream<ap_uint<DATAWIDTH> > soc2xbar_data("soc2xbar_data"), xbar2soc_data("xbar2soc_data");
	stream<ap_uint<DATAWIDTH> > extep2xbar_data("extep2xbar_data"), xbar2extep_data("xbar2extep_data");
	stream<struct lego_mem_ctrl> soc2xbar_ctrl, xbar2soc_ctrl;
	stream<struct lego_mem_ctrl> extep2xbar_ctrl, xbar2extep_ctrl;
	bool phase_done = false, delay_done = false;
	int counter = 0;

	cout << "\nTesting multi version data store module" << endl;
	cout << "1. Get client PID" << endl;
	net.state_reset();
	do {
		phase_done = net.net_sim_proc_create(net2xbar, xbar2net);
		multi_ver_obj2(xbar2extep_ctrl, extep2xbar_ctrl, xbar2extep_data, extep2xbar_data);
		mem.mem_sim(xbar2mem, mem2xbar);
		soc.soc_sim(xbar2soc_ctrl, soc2xbar_ctrl, xbar2soc_data, soc2xbar_data);
		xbar.xbar_sim(xbar2extep_ctrl, extep2xbar_ctrl, xbar2soc_ctrl, soc2xbar_ctrl,
			      xbar2net, net2xbar, xbar2extep_data, extep2xbar_data,
			      xbar2soc_data, soc2xbar_data, xbar2mem, mem2xbar);
		delay_done = delay_cycles(phase_done, max(soc_datapath_latency, mem_datapath_latency) * 10);
	} while (!phase_done || !delay_done);

	cout << "\n2. Create " << object_count << " Objects" << endl;
	phase_done = false;
	net.state_reset();
	do {
		multi_ver_obj2(xbar2extep_ctrl, extep2xbar_ctrl, xbar2extep_data, extep2xbar_data);
		if (counter++ % tb_slowdowm == 0) {
		phase_done = net.net_sim_obj_create(net2xbar, xbar2net);
		mem.mem_sim(xbar2mem, mem2xbar);
		soc.soc_sim(xbar2soc_ctrl, soc2xbar_ctrl, xbar2soc_data, soc2xbar_data);
		xbar.xbar_sim(xbar2extep_ctrl, extep2xbar_ctrl, xbar2soc_ctrl, soc2xbar_ctrl,
			      xbar2net, net2xbar, xbar2extep_data, extep2xbar_data,
			      xbar2soc_data, soc2xbar_data, xbar2mem, mem2xbar);
		delay_done = delay_cycles(phase_done, max(soc_datapath_latency, mem_datapath_latency) * 2);
		}
	} while (!phase_done || !delay_done);

	cout << "\n3. Write every object once. In total " << object_count << endl;
	phase_done = false;
	net.state_reset();
	do {
		multi_ver_obj2(xbar2extep_ctrl, extep2xbar_ctrl, xbar2extep_data, extep2xbar_data);
		if (counter++ % tb_slowdowm == 0) {
		phase_done = net.net_sim_write_once(net2xbar, xbar2net);
		mem.mem_sim(xbar2mem, mem2xbar);
		soc.soc_sim(xbar2soc_ctrl, soc2xbar_ctrl, xbar2soc_data, soc2xbar_data);
		xbar.xbar_sim(xbar2extep_ctrl, extep2xbar_ctrl, xbar2soc_ctrl, soc2xbar_ctrl,
			      xbar2net, net2xbar, xbar2extep_data, extep2xbar_data,
			      xbar2soc_data, soc2xbar_data, xbar2mem, mem2xbar);
		delay_done = delay_cycles(phase_done, max(soc_datapath_latency, mem_datapath_latency) * 2);
		}
	} while (!phase_done || !delay_done);

	cout << "\n4. Read/Write pattern test" << endl;
	phase_done = false;
	net.state_reset();
	do {
		multi_ver_obj2(xbar2extep_ctrl, extep2xbar_ctrl, xbar2extep_data, extep2xbar_data);
		if (counter++ % tb_slowdowm == 0) {
		phase_done = net.net_sim(net2xbar, xbar2net);
		mem.mem_sim(xbar2mem, mem2xbar);
		soc.soc_sim(xbar2soc_ctrl, soc2xbar_ctrl, xbar2soc_data, soc2xbar_data);
		xbar.xbar_sim(xbar2extep_ctrl, extep2xbar_ctrl, xbar2soc_ctrl, soc2xbar_ctrl,
			      xbar2net, net2xbar, xbar2extep_data, extep2xbar_data,
			      xbar2soc_data, soc2xbar_data, xbar2mem, mem2xbar);
		delay_done = delay_cycles(phase_done, max(soc_datapath_latency, mem_datapath_latency) * 2);
		}
	} while (!phase_done || !delay_done);

	cout << "\nAll tests SUCCESS!!!" << endl;
}

int main() {
#if 0
	version_bram_test_suite bram_test(6);
	//bram_test.objid_pattern.print_pattern();
	return bram_test.version_bram_main_loop();
#else
	const int num_client = 2;
	const int object_count = 6;
	const int soc_datapath_latency = 8;
	const int soc_ctrlpath_latency = 12;
	const int mem_datapath_latency = 10;
	/* co-sim have read while empty error without slowdown (maybe compiler bug?) */
	const int tb_slowdowm = 8;
	whole_design_loop(num_client, object_count, soc_datapath_latency,
			soc_datapath_latency, mem_datapath_latency, tb_slowdowm);
	return 0;
#endif
}
