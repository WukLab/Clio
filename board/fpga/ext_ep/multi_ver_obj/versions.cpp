/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "multi_ver_obj.h"

using namespace hls;

void
bram2p(stream<struct version_bram_if> &in1, stream<struct version_bram_if> &out1,
       stream<struct version_bram_if> &in2, stream<struct version_bram_if> &out2)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	static uint16_t version_idxs[OBJ_ARRAY_COUNT];
#pragma HLS RESOURCE variable=version_idxs core=RAM_T2P_BRAM
#pragma HLS DEPENDENCE variable=version_idxs inter false
#pragma HLS DEPENDENCE variable=version_idxs intra false

	if (!in1.empty()) {
		struct version_bram_if req1, resp1;
		req1 = in1.read();
		if (req1.rw == VERSION_READ) {
			resp1.rw = VERSION_READ;
			resp1.obj_id = req1.obj_id;
			resp1.version = version_idxs[req1.obj_id];
			out1.write(resp1);
		} else if (req1.rw == VERSION_WRITE) {
			version_idxs[req1.obj_id] = req1.version;
		} else {
			assert(false);
		}
	}

	if (!in2.empty()) {
		struct version_bram_if req2, resp2;
		req2 = in2.read();
		if (req2.rw == VERSION_READ) {
			resp2.rw = VERSION_READ;
			resp2.obj_id = req2.obj_id;
			resp2.version = version_idxs[req2.obj_id];
			out2.write(resp2);
		} else if (req2.rw == VERSION_WRITE) {
			version_idxs[req2.obj_id] = req2.version;
		} else {
			assert(false);
		}
	}
}

static void
stage2(stream<struct version_bram_if> &in, stream<struct version_bram_if> &out,
       stream<struct version_bram_if> &version_in, stream<struct version_bram_if> &bram_cmd)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off
	static struct delay<ap_uint<16> > current_wip_id        = {0,0};

	if (!in.empty() && !version_in.empty()) {
		struct version_bram_if cmd = {0,0,0};
		struct version_bram_if req = in.read();
		struct version_bram_if prev_req_done = version_in.read();

		if (current_wip_id.vld) {
			if (current_wip_id.data == req.obj_id)
				req.version++;
			current_wip_id.vld = 0;
		}

		if (req.rw == VERSION_INC) {
			req.version += prev_req_done.version;
			cmd.obj_id = req.obj_id;
			cmd.rw = VERSION_WRITE;
			cmd.version = req.version;
			bram_cmd.write(cmd);

			current_wip_id.data = req.obj_id;
			current_wip_id.vld = 1;
		} else {
			req.version += prev_req_done.version;
		}
		out.write(req);
	} else {
		current_wip_id.vld = 0;
	}
}

static void
stage1(stream<struct version_bram_if> &in, stream<struct version_bram_if> &out,
	stream<struct version_bram_if> &bram_cmd)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	if (!in.empty()) {
		struct version_bram_if cmd = {0,0,0};
		struct version_bram_if req = in.read();
		assert(req.version == 0);

		if (req.rw == VERSION_INC)
			req.version++;

		cmd.obj_id = req.obj_id;
		cmd.rw = VERSION_READ;
		bram_cmd.write(cmd);
		out.write(req);
	}
}

/*
 * since other memory request is definitely longer, it's okay to have more cycles
 * read request with version specified should not send request here
 */
void version_idxs(stream<struct version_bram_if> &in, stream<struct version_bram_if> &out)
{
#pragma HLS DATAFLOW

	static stream<struct version_bram_if> stage1_2stage2("stage1_2stage2");
	static stream<struct version_bram_if> bram_cmd1("bram_cmd1");
	static stream<struct version_bram_if> bram_cmd2("bram_cmd2");
	static stream<struct version_bram_if> bram_out("bram_out");
	static stream<struct version_bram_if> arbiter2bram("arbiter2bram");
	static stream<struct version_bram_if> bram_out2("bram_out2");

#pragma HLS DATA_PACK variable=stage1_2stage2
#pragma HLS DATA_PACK variable=bram_cmd1
#pragma HLS DATA_PACK variable=bram_cmd2
#pragma HLS DATA_PACK variable=bram_out
#pragma HLS DATA_PACK variable=bram_out2
#pragma HLS DATA_PACK variable=arbiter2bram

#pragma HLS STREAM variable=stage1_2stage2		depth=8
#pragma HLS STREAM variable=bram_cmd1			depth=8
#pragma HLS STREAM variable=bram_cmd2			depth=8
#pragma HLS STREAM variable=bram_out			depth=8
#pragma HLS STREAM variable=arbiter2bram		depth=8

	stage1(in, stage1_2stage2, bram_cmd1);
	stage2(stage1_2stage2, out, bram_out, bram_cmd2);
	bram2p(bram_cmd1, bram_out, bram_cmd2, bram_out2);
}

void version_idxs2(stream<struct version_bram_if> &in, stream<struct version_bram_if> &out)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	static uint16_t version_idxs[OBJ_ARRAY_COUNT];
#pragma HLS RESOURCE variable=version_idxs core=RAM_S2P_URAM

	if (!in.empty()) {
		struct version_bram_if req, resp;
		int version;
		req = in.read();
		if (req.rw == VERSION_READ) {
			resp.rw = VERSION_READ;
			resp.obj_id = req.obj_id;
			version = version_idxs[req.obj_id];
			if (version < req.version)
				resp.version = version + VERSION_ARRAY_COUNT - req.version;
			else
				resp.version = version - req.version;
			out.write(resp);
		} else if (req.rw == VERSION_INC) {
			version = version_idxs[req.obj_id];
			if (version == VERSION_ARRAY_COUNT - 1)
				version_idxs[req.obj_id] = 0;
			else
				version_idxs[req.obj_id] = version + 1;
			resp.rw = VERSION_INC;
			resp.obj_id = req.obj_id;
			resp.version = version_idxs[req.obj_id];
			out.write(resp);
		} else {
			assert(false);
		}
	}
}
