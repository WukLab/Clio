/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "multi_ver_obj.h"
#include "multi_ver_obj_ops.h"
#include "multi_ver_obj_bramdata.h"

using namespace hls;

void multiver_obj(stream<struct ctrl_if> &ctrl_in, stream<struct ctrl_if> &ctrl_out,
		  stream<struct data_if> &data_in, stream<struct data_if> &data_out)
{
// remove ap_ctrl_none before doing cosim test
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE port=ctrl_in axis
#pragma HLS INTERFACE port=ctrl_out axis
#pragma HLS INTERFACE port=data_in axis
#pragma HLS INTERFACE port=data_out axis
#pragma HLS DATAFLOW

	static stream<struct record_out_if> soc_wip("soc_wip");
	static stream<struct record_out_if> mem_wip("mem_wip");
	static stream<struct data_if> dispatch2parser("dispatch2parser");
	static stream<struct data_if> dispatch2wq1("dispatch2wq1");
	static stream<struct data_if> dispatch2wq2("dispatch2wq2");
	static stream<struct data_if> dispatch2sq("dispatch2sq");
	static stream<struct version_bram_if> parser2bram("parser2bram");
	static stream<struct data_record_if> parser2sq_data1("parser2sq_data1");
	static stream<struct data_record_if> parser2sq_data2("parser2sq_data2");
	static stream<struct wait_if> parser2wq1("parser2wq1");
	static stream<struct version_bram_if> bram2wq1("bram2wq1");
	static stream<struct wait_if> wq1_2wq2("wq1_2wq2");
	static stream<struct data_if> wq1_2sq_net("wq1_2sq_net");
	static stream<struct data_record_if> wq1_2sq_soc("wq1_2sq_soc");
	static stream<struct data_record_if> wq1_2sq_mem("wq1_2sq_mem");
	static stream<struct data_if> wq2_2sq_net("wq2_2sq_net");
	static stream<struct data_record_if> wq2_2sq_mem("wq2_2sq_mem");

#pragma HLS DATA_PACK variable=soc_wip
#pragma HLS DATA_PACK variable=mem_wip
#pragma HLS DATA_PACK variable=dispatch2parser
#pragma HLS DATA_PACK variable=dispatch2wq1
#pragma HLS DATA_PACK variable=dispatch2wq2
#pragma HLS DATA_PACK variable=dispatch2sq
#pragma HLS DATA_PACK variable=parser2bram
#pragma HLS DATA_PACK variable=parser2sq_data1
#pragma HLS DATA_PACK variable=parser2sq_data2
#pragma HLS DATA_PACK variable=parser2wq1
#pragma HLS DATA_PACK variable=bram2wq1
#pragma HLS DATA_PACK variable=wq1_2wq2
#pragma HLS DATA_PACK variable=wq1_2sq_net
#pragma HLS DATA_PACK variable=wq1_2sq_soc
#pragma HLS DATA_PACK variable=wq1_2sq_mem
#pragma HLS DATA_PACK variable=wq2_2sq_net
#pragma HLS DATA_PACK variable=wq2_2sq_mem

/*
 * Assumption:
 * DATA span at most 9 cycles ~ 4Kbits
 * (at the time of designing, I only consider 1G memory, >10K entry, 1024 version,
 * average data length can only be ~ 4Kbits)
 *
 * order of description below is based on order of consideration,
 * basically, FIFO closer to output port should be considered first
 *
 * dispatch2sq: post high priority in sendqueue, so if sendqueue is sending packet from other
 *     source, it will be the next. Since the longest wait will be on waiting waitqueue1 sending
 *     out write data. Also, while dispatcher is forwarding reply out, there will be no request
 *     filling out other FIFO. So decided with assumption above, latency of dispatcher,
 *     latency of sendqueue, and with some relaxation
 * wq2_2sq_mem: this only happens for CREATE request, metadata written to memory at the end of
 *     CREATE request. this FIFO is not used to hold requests, so can be shorter. It has second
 *     priority in sendqueue after dispatcher. However, since while dispatcher occupying sendqueue,
 *     no other request will come in, so length of FIFO don't have to be based on size of dispatch2sq.
 *     Decided with latency of wq2, and sendqueue, and with some relaxation
 * wq2_2sq_net: same as wq2_2sq_mem, but send after wq2_2sq_mem, so FIFO basically have to be twice
 *     as large as wq2_2sq_mem
 * wq1_2sq_mem: worst case, WRITE request metadata read reply at once, and DATA is large. But since
 *     DATA is actually storing in parser2wq1, so if sendqueue is selecting this port, data will just
 *     flow through this fifo. However, since it's priority is lower than waitqueue2, its size should
 *     be larger than two waitqueue2 fifo.
 * wq1_2sq_soc: similar to wq1_2sq_mem, only happens for DELETE request, so having size similar to wq1_2sq_mem
 * wq1_2sq_net: similar to wq1_2sq_mem, only happens for DELETE request, so having size similar to wq1_2sq_mem
 * parser2sq_data1: sendqueue priority is lowest, but once selected, data can just flow through,
 *     so a little larger than wq1_2sq_mem is enough
 * parser2sq_data2: same as parser2sq_data1, send after parser2sq_data2, so larger than parser2sq_data1
 *
 * dispatch2parser: parser is able to handle request every II, having a small fifo is enough
 * dispatch2wq1: waitqueue1 is able to handle request every II, having a small fifo is enough
 * dispatch2wq2: waitqueue2 is able to handle request every II, having a small fifo is enough
 * parser2bram: BRAM can handle request every II, so having a small fifo is enough
 *
 * belows are FIFO waiting for external request (SoC and Core Mem), so FIFO size should based on latency of
 * other component
 * parser2wq1: should be the largest FIFO, all request at some time, storing at here
 * wq1_2wq2: only create request is waiting here, so can be smaller
 * bram2wq1: holding the version number read from BRAM, will be used by waitqueue1, so be the same as parser2wq1,
 *     it's not a large structure, so a little deeper won't hurt resource utilization
 * soc_wip: all request to SoC have a record here, so making it larger, it's not a large structure,
 *     so a little deeper won't hurt resource utilization
 * mem_wip: all request to Mem have a record here, so making it even larger, it's not a large structure,
 *     so a little deeper won't hurt resource utilization
 */

#pragma HLS STREAM variable=soc_wip		depth=40
#pragma HLS STREAM variable=mem_wip		depth=500
#pragma HLS STREAM variable=dispatch2parser	depth=8
#pragma HLS STREAM variable=dispatch2wq1	depth=8
#pragma HLS STREAM variable=dispatch2wq2	depth=8
#pragma HLS STREAM variable=dispatch2sq		depth=16
#pragma HLS STREAM variable=parser2bram		depth=4
#pragma HLS STREAM variable=parser2sq_data1	depth=24
#pragma HLS STREAM variable=parser2sq_data2	depth=28
#pragma HLS STREAM variable=parser2wq1		depth=500
#pragma HLS STREAM variable=bram2wq1		depth=70
#pragma HLS STREAM variable=wq1_2wq2		depth=500
#pragma HLS STREAM variable=wq1_2sq_net		depth=20
#pragma HLS STREAM variable=wq1_2sq_soc		depth=20
#pragma HLS STREAM variable=wq1_2sq_mem		depth=20
#pragma HLS STREAM variable=wq2_2sq_net		depth=8
#pragma HLS STREAM variable=wq2_2sq_mem		depth=4

#pragma HLS RESOURCE variable=soc_wip 		core=FIFO_SRL
#pragma HLS RESOURCE variable=mem_wip 		core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=dispatch2parser 	core=FIFO_SRL
#pragma HLS RESOURCE variable=dispatch2wq1 	core=FIFO_SRL
#pragma HLS RESOURCE variable=dispatch2wq2 	core=FIFO_SRL
#pragma HLS RESOURCE variable=dispatch2sq 	core=FIFO_SRL
#pragma HLS RESOURCE variable=parser2bram 	core=FIFO_SRL
#pragma HLS RESOURCE variable=parser2sq_data1 	core=FIFO_SRL
#pragma HLS RESOURCE variable=parser2sq_data2 	core=FIFO_SRL
#pragma HLS RESOURCE variable=parser2wq1 	core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=bram2wq1 		core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=wq1_2wq2 		core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=wq1_2sq_net 	core=FIFO_SRL
#pragma HLS RESOURCE variable=wq1_2sq_soc 	core=FIFO_SRL
#pragma HLS RESOURCE variable=wq1_2sq_mem	core=FIFO_SRL
#pragma HLS RESOURCE variable=wq2_2sq_net 	core=FIFO_SRL
#pragma HLS RESOURCE variable=wq2_2sq_mem 	core=FIFO_SRL

	dispatcher(data_in, ctrl_in, ctrl_out, soc_wip, mem_wip,
		dispatch2parser, dispatch2wq1, dispatch2wq2, dispatch2sq);
	parser(dispatch2parser, parser2bram, parser2sq_data1, parser2sq_data2, parser2wq1);
	version_idxs2(parser2bram, bram2wq1);
	waitqueue1(parser2wq1, wq1_2wq2, wq1_2sq_net, wq1_2sq_soc, wq1_2sq_mem,
		dispatch2wq1, bram2wq1);
	waitqueue2(wq1_2wq2, dispatch2wq2, wq2_2sq_net, wq2_2sq_mem);
	sendqueue(parser2sq_data1, parser2sq_data2, wq1_2sq_net, wq1_2sq_soc,
		wq1_2sq_mem, wq2_2sq_net, wq2_2sq_mem, dispatch2sq, soc_wip, mem_wip, data_out);
}
