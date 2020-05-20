/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <iostream>
#include <stdlib.h>
#include <stdexcept>
#include <tb_sim.h>
#include <uapi/compiler.h>

//#define SIM_MEM_PRINT

using namespace hls;
using namespace std;

MemSim::MemSim(unsigned long size, unsigned int latency)
	: mem_size(size),
	  latency(latency)
{
	delay_fifo.resize(latency);
	memory.resize(size);
	state = MEM_HEADER;
	rw_addr = 0;
	rw_size = 0;
	write_resp_wait	= {0,0};
	cout << "Simulated Memory Initialized" << endl;
}

void MemSim::mem_sim(stream<ap_uint<DATAWIDTH> > &data_in, stream<ap_uint<DATAWIDTH> > &data_out)
{
	static stream<ap_uint<DATAWIDTH> > parse2delay("parse2delay");

	parse(data_in, parse2delay);
	delay(parse2delay, data_out);
}

struct coremem_data {
	uint8_t				data[DATASIZE];
};
void MemSim::parse(stream<ap_uint<DATAWIDTH> > &data_in, stream<ap_uint<DATAWIDTH> > &to_delay)
{
	ap_uint<DATAWIDTH> in_pkt 				= 0;
	ap_uint<DATAWIDTH> out_pkt 				= 0;
	struct legomem_rw_fpgamsg *req;
	struct legomem_rw_fpgamsg_resp resp 			= {0};
	struct coremem_data *rep_data;
	struct coremem_data resp_data				= {0};

	switch (state) {
	case MEM_HEADER:
		if (data_in.empty())
			break;
		in_pkt = data_in.read();
		req = (struct legomem_rw_fpgamsg *)&in_pkt;

#ifdef SIM_MEM_PRINT
		std::cout << "MEM HDR:   " << std::hex << in_pkt << std::endl;
#endif

		rw_addr = req->op.va;
		rw_size = req->op.size;
		switch (req->hdr.opcode) {
		case OP_REQ_READ:
			if (req->hdr.size != sizeof(struct legomem_rw_fpgamsg))
				throw invalid_argument("hdr.size for OP_REQ_READ is wrong");

			resp.hdr = req->hdr;
			resp.hdr.opcode = OP_REQ_READ_RESP;
			resp.hdr.cont = LEGOMEM_CONT_EXTAPI;

			/* out-of-range: return error message */
			if (rw_addr + rw_size >= mem_size) {
				resp.hdr.size = sizeof(struct legomem_rw_fpgamsg_resp);
				resp.hdr.req_status = 0xF;
				memcpy(&out_pkt, &resp, DATASIZE);
				to_delay.write(out_pkt);
				break;
			}

			resp.hdr.size = sizeof(struct legomem_rw_fpgamsg_resp) + rw_size;
			resp.hdr.req_status = 0;
			if (rw_size <= COREMEMRETDATASIZE) {
				rwmem(0, rw_addr, rw_size, (char*)resp.op.data);
			} else {
				rwmem(0, rw_addr, COREMEMRETDATASIZE, (char*)resp.op.data);
				rw_addr += COREMEMRETDATASIZE;
				rw_size -= COREMEMRETDATASIZE;
				state = MEM_READDATA;
			}
			memcpy(&out_pkt, &resp, DATASIZE);
			to_delay.write(out_pkt);
#ifdef SIM_MEM_PRINT
		std::cout << "MEM READ:  " << std::hex << out_pkt << std::endl;
#endif
			break;

		case OP_REQ_WRITE:
			if (req->hdr.size != sizeof(struct legomem_rw_fpgamsg) + rw_size)
				throw invalid_argument("hdr.size for OP_REQ_WRITE is wrong");

			resp.hdr = req->hdr;
			resp.hdr.opcode = OP_REQ_WRITE_RESP;
			resp.hdr.cont = LEGOMEM_CONT_EXTAPI;
			resp.hdr.size = sizeof(struct legomem_rw_fpgamsg_resp);

			/* out-of-range: return error message */
			if (rw_addr + rw_size >= mem_size) {
				resp.hdr.req_status = 0xF;
				memcpy(&out_pkt, &resp, DATASIZE);
				to_delay.write(out_pkt);
				break;
			}

			resp.hdr.req_status = 0;
			if (rw_size <= COREMEMDATASIZE) {
				rwmem(1, rw_addr, rw_size, (char*)req->op.data);
				memcpy(&out_pkt, &resp, DATASIZE);
				to_delay.write(out_pkt);
			} else {
				rwmem(1, rw_addr, COREMEMDATASIZE, (char*)req->op.data);
				memcpy(&write_resp_wait, &resp, sizeof(struct legomem_rw_fpgamsg_resp));
				rw_addr += COREMEMDATASIZE;
				rw_size -= COREMEMDATASIZE;
				state = MEM_WRITEDATA;
			}
			break;

		default:
			throw invalid_argument("bad opcode");
			break;
		}
		break;

	case MEM_WRITEDATA:
		if (data_in.empty())
			break;
		in_pkt = data_in.read();
		rep_data = (struct coremem_data *)&in_pkt;

#ifdef SIM_MEM_PRINT
		std::cout << "MEM WRITE: " << std::hex << in_pkt << std::endl;
#endif
		if (rw_size <= DATASIZE) {
			rwmem(1, rw_addr, rw_size, (char*)rep_data->data);
			memcpy(&out_pkt, &write_resp_wait, sizeof(struct legomem_rw_fpgamsg_resp));
			to_delay.write(out_pkt);
			rw_addr = 0;
			rw_size = 0;
			state = MEM_HEADER;
		} else {
			rwmem(1, rw_addr, DATASIZE, (char*)rep_data->data);
			rw_addr += DATASIZE;
			rw_size -= DATASIZE;
		}
		break;

	case MEM_READDATA:
		if (rw_size <= DATASIZE) {
			rwmem(0, rw_addr, rw_size, (char*)resp_data.data);
			state = MEM_HEADER;
		} else {
			rwmem(0, rw_addr, DATASIZE, (char*)resp_data.data);
			rw_addr += DATASIZE;
			rw_size -= DATASIZE;
		}
		memcpy(&out_pkt, &resp_data, DATASIZE);
#ifdef SIM_MEM_PRINT
		std::cout << "MEM READ:  " << std::hex << out_pkt << std::endl;
#endif
		to_delay.write(out_pkt);
		break;

	default:
		break;
	}
}

void MemSim::delay(stream<ap_uint<DATAWIDTH> > &from_parse, stream<ap_uint<DATAWIDTH> > &data_out)
{
	for (int i = latency - 1; i >= 1; i--) {
		if (i == latency - 1 && delay_fifo[i].vld) {
			data_out.write(delay_fifo[i].pkt);
		}
		delay_fifo[i] = delay_fifo[i-1];
	}

	if (from_parse.empty()) {
		delay_fifo[0].pkt = 0;
		delay_fifo[0].vld = 0;
	} else {
		delay_fifo[0].pkt = from_parse.read();
		delay_fifo[0].vld = 1;
	}
}

/*
 * read: 0
 * write: 1
 */
void MemSim::rwmem(bool rw, unsigned long addr, unsigned long size, char *data)
{
	assert(addr < mem_size);
	if (rw == 0)
		memcpy(data, reinterpret_cast<char*>(&memory[addr]), size);
	else
		memcpy(reinterpret_cast<char*>(&memory[addr]), data, size);
}
