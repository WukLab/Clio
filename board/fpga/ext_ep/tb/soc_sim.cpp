/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <tb_sim.h>
#include <iostream>
#include <stdexcept>

using namespace hls;
using namespace std;

SocSim::SocSim(unsigned long mem_size,
	       unsigned int data_latency, unsigned int ctrl_latency)
	: mem_size(mem_size),
	  data_latency(data_latency),
	  ctrl_latency(ctrl_latency),
	  parse2datadelay("parse2datadelay"),
	  parse2ctrldelay("parse2ctrldelay")
{
	// use a relative large number less than 256 to help debugging
	pid = 128;
	alloc_map = 0x4000;
	data_delay_fifo.resize(data_latency);
	ctrl_delay_fifo.resize(ctrl_latency);
	cout << "Simulated SoC Initialized" << endl;
}

void SocSim::soc_sim(stream<struct lego_mem_ctrl> &ctrl_in,
		     stream<struct lego_mem_ctrl> &ctrl_out,
		     stream<ap_uint<DATAWIDTH> > &data_in,
		     stream<ap_uint<DATAWIDTH> > &data_out)
{
	parse(ctrl_in, parse2ctrldelay, data_in, parse2datadelay);
	ctrl_delay(parse2ctrldelay, ctrl_out);
	data_delay(parse2datadelay, data_out);
}

void SocSim::parse(stream<struct lego_mem_ctrl> &ctrl_in,
		   stream<struct lego_mem_ctrl> &ctrl_to_delay,
		   stream<ap_uint<DATAWIDTH> > &data_in,
		   stream<ap_uint<DATAWIDTH> > &data_to_delay)
{
	// temporary variable
	struct lego_mem_ctrl inctrl_pkt = {0}, outctrl_pkt = {0};
	ap_uint<DATAWIDTH> indata_pkt = 0, outdata_pkt = 0;
	struct legomem_alloc_free_fpgamsg *req;
	struct legomem_alloc_free_fpgamsg_resp resp = {0};
	struct sim_used_proc_create *create;
	struct sim_used_proc_create_ret create_ret = {0};

	// ctrl path
	if(ctrl_in.empty())
		goto datapath;

	inctrl_pkt = ctrl_in.read();
	switch (inctrl_pkt.cmd) {
	case CMD_LEGOMEM_CTRL_CREATE_PROC:
		outctrl_pkt.param32 = pid;
		outctrl_pkt.param8 = 0;
		outctrl_pkt.beats = inctrl_pkt.beats;
		outctrl_pkt.cmd = inctrl_pkt.cmd;
		outctrl_pkt.addr = EXTAPI_XBAR_ADDR;
		outctrl_pkt.epid = EXTAPI_XBAR_EPID;
		pid++;
		ctrl_to_delay.write(outctrl_pkt);
		break;

	case CMD_LEGOMEM_CTRL_ALLOC:
		outctrl_pkt.param32 = alloc_map;
		outctrl_pkt.param8 = 0;
		outctrl_pkt.beats = inctrl_pkt.beats;
		outctrl_pkt.cmd = inctrl_pkt.cmd;
		outctrl_pkt.addr = EXTAPI_XBAR_ADDR;
		outctrl_pkt.epid = EXTAPI_XBAR_EPID;
		// check overflow
		if (alloc_map + inctrl_pkt.param32 >= mem_size)
			throw overflow_error("memory overflow, please enlarge \"mem_size\"");
		alloc_map += inctrl_pkt.param32;
		ctrl_to_delay.write(outctrl_pkt);
		break;

	case CMD_LEGOMEM_CTRL_FREE:
		throw logic_error("simple allocator, don't have ctrlpath free for now");
		break;

	default:
		throw invalid_argument("bad ctrl path cmd");
		break;
	}

	// data path
datapath:
	if (data_in.empty())
		return;
	indata_pkt = data_in.read();
	switch (field(indata_pkt, hdr_opcode)) {
	case OP_REQ_ALLOC:
		req = (struct legomem_alloc_free_fpgamsg *)&indata_pkt;
		resp.hdr = req->hdr;
		resp.hdr.opcode = OP_REQ_ALLOC_RESP;
		resp.hdr.cont = LEGOMEM_CONT_EXTAPI;
		resp.hdr.size = sizeof(struct legomem_alloc_free_fpgamsg_resp);
		if (alloc_map + req->op.len >= mem_size) {
			resp.hdr.req_status = 0xF;
			resp.op.ret = 0xF;
		} else {
			resp.hdr.req_status = 0;
			resp.op.ret = 0;
			resp.op.addr = alloc_map;
			alloc_map += req->op.len;
		}
		memcpy(&outdata_pkt, &resp, sizeof(struct legomem_alloc_free_fpgamsg_resp));
		data_to_delay.write(outdata_pkt);
		break;

	case OP_REQ_FREE:
		req = (struct legomem_alloc_free_fpgamsg *)&indata_pkt;
		resp.hdr = req->hdr;
		resp.hdr.opcode = OP_REQ_FREE_RESP;
		resp.hdr.cont = LEGOMEM_CONT_EXTAPI;
		resp.hdr.size = sizeof(struct legomem_alloc_free_fpgamsg_resp);
		resp.hdr.req_status = 0;
		resp.op.ret = 0;
		memcpy(&outdata_pkt, &resp, sizeof(struct legomem_alloc_free_fpgamsg_resp));
		data_to_delay.write(outdata_pkt);
		break;

	/* just to get an PID */
	case OP_CREATE_PROC:
		create = (struct sim_used_proc_create *)&indata_pkt;
		create_ret.hdr = create->hdr;
		create_ret.hdr.opcode = OP_CREATE_PROC_RESP;
		create_ret.hdr.cont = LEGOMEM_CONT_NET;
		create_ret.hdr.size = sizeof(struct sim_used_proc_create_ret);
		create_ret.hdr.req_status = 0;
		create_ret.op.ret = 0;
		create_ret.op.pid = pid;
		memcpy(&outdata_pkt, &create_ret, sizeof(struct sim_used_proc_create_ret));
		data_to_delay.write(outdata_pkt);
		pid++;
		break;

	default:
		throw invalid_argument("bad data path opcode");
		break;
	}
}

void SocSim::data_delay(stream<ap_uint<DATAWIDTH> > &from_parse,
			stream<ap_uint<DATAWIDTH> > &data_out)
{
	for (int i = data_latency - 1; i >= 1; i--) {
		if (i == data_latency - 1 && data_delay_fifo[i].vld) {
			data_out.write(data_delay_fifo[i].pkt);
		}
		data_delay_fifo[i] = data_delay_fifo[i-1];
	}

	if (from_parse.empty()) {
		data_delay_fifo[0].pkt = 0;
		data_delay_fifo[0].vld = 0;
	} else {
		data_delay_fifo[0].pkt = from_parse.read();
		data_delay_fifo[0].vld = 1;
	}
}

void SocSim::ctrl_delay(stream<struct lego_mem_ctrl> &from_parse,
			stream<struct lego_mem_ctrl> &ctrl_out)
{
	for (int i = ctrl_latency - 1; i >= 1; i--) {

		if (i == ctrl_latency - 1 && ctrl_delay_fifo[i].vld) {
			ctrl_out.write(ctrl_delay_fifo[i].pkt);
		}
		ctrl_delay_fifo[i] = ctrl_delay_fifo[i-1];
	}

	if (from_parse.empty()) {
		ctrl_delay_fifo[0].pkt = {0,0,0,0,0,0};
		ctrl_delay_fifo[0].vld = 0;
	} else {
		ctrl_delay_fifo[0].pkt = from_parse.read();
		ctrl_delay_fifo[0].vld = 1;
	}
}
