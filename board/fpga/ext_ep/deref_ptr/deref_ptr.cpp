/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <fifo.h>
#include "deref_ptr.h"

using namespace hls;

static void
decode(stream<ap_uint<DATAWIDTH> > &data_in,
       stream<struct data_if> &to_sendqueue,
       stream<struct data_if> &to_waitqueue,
       stream<struct release_if> &waitqueue_release)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum decode_fsm {PARSE, DATATOWAITQUEUE, DATATOWAITQUEUE2, DATATOSENDQUEUE};

	// stateful variable, remain the same between cycles
	static ap_uint<16> data_remain				= 0;
	static decode_fsm state					= PARSE;
	static struct data_if wait_pkt				= {0,0};
	static FIFO<uint8_t> seqid_wq;

	// delay packet for one cycle to meet timing
	static struct delay<struct data_if> to_waitqueue_delay	= {0,0};
	static struct delay<struct data_if> to_sendqueue_delay	= {0,0};
	static struct delay<struct release_if>  release_delay	= {0,0};

	// temporary variable
	ap_uint<DATAWIDTH> in_pkt			= 0;
	struct data_if send_pkt				= {0,0};
	struct release_if release			= {0,0};

	/* delay one cycle logic */
	if (to_sendqueue_delay.vld) {
		to_sendqueue.write(to_sendqueue_delay.data);
		to_sendqueue_delay.vld = 0;
	}

	if (to_waitqueue_delay.vld) {
		to_waitqueue.write(to_waitqueue_delay.data);
		to_waitqueue_delay.vld = 0;
	}

	if (release_delay.vld) {
		waitqueue_release.write(release_delay.data);
		release_delay.vld = 0;
	}

	switch (state) {
	case PARSE:
		if (data_in.empty())
			break;
		in_pkt = data_in.read();

		switch (in_pkt.range(hdr_opcode_up, hdr_opcode_lo)) {
		case OP_REQ_IREAD:
			wait_pkt.pkt = 0;
			wait_pkt.last = 0;

			// for deref request, first half request to core mem
			send_pkt.pkt.range(hdr_up, hdr_lo) = in_pkt.range(hdr_up, hdr_lo);
			send_pkt.pkt.range(hdr_opcode_up, hdr_opcode_lo) = OP_REQ_READ;
			send_pkt.pkt.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_read_write_req);
			send_pkt.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			send_pkt.pkt.range(mem_va_up, mem_va_lo)
				= in_pkt.range(deref_addr_up, deref_addr_lo)
				+ in_pkt.range(deref_off1_up, deref_off1_lo);
			send_pkt.pkt.range(mem_size_up, mem_size_lo) = sizeof(void *);
			send_pkt.last = 1;
			delay_pkt(to_sendqueue_delay, send_pkt);

			// second half to wait
			wait_pkt.pkt.range(hdr_up, hdr_lo) = in_pkt.range(hdr_up, hdr_lo);
			wait_pkt.pkt.range(hdr_opcode_up, hdr_opcode_lo) = OP_REQ_READ;
			wait_pkt.pkt.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_read_write_req);
			wait_pkt.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			// filled with offset
			wait_pkt.pkt.range(mem_va_up, mem_va_lo)
				= in_pkt.range(deref_off2_up, deref_off2_lo);
			wait_pkt.pkt.range(mem_size_up, mem_size_lo)
				= in_pkt.range(deref_size_up, deref_size_lo);
			wait_pkt.last = 1;
			delay_pkt(to_waitqueue_delay, wait_pkt);
			seqid_wq.push(in_pkt.range(hdr_seqId_up, hdr_seqId_lo));

			break;

		case OP_REQ_IWRITE:
			wait_pkt.pkt = 0;
			wait_pkt.last = 0;

			// for deref request, first half request to core mem
			send_pkt.pkt.range(hdr_up, hdr_lo) = in_pkt.range(hdr_up, hdr_lo);
			send_pkt.pkt.range(hdr_opcode_up, hdr_opcode_lo) = OP_REQ_READ;
			send_pkt.pkt.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_read_write_req);
			send_pkt.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			send_pkt.pkt.range(mem_va_up, mem_va_lo)
				= in_pkt.range(deref_addr_up, deref_addr_lo)
				+ in_pkt.range(deref_off1_up, deref_off1_lo);
			send_pkt.pkt.range(mem_size_up, mem_size_lo) = sizeof(void *);
			send_pkt.last = 1;
			delay_pkt(to_sendqueue_delay, send_pkt);

			// second half to wait
			wait_pkt.pkt.range(hdr_up, hdr_lo) = in_pkt.range(hdr_up, hdr_lo);
			wait_pkt.pkt.range(hdr_opcode_up, hdr_opcode_lo) = OP_REQ_WRITE;
			wait_pkt.pkt.range(hdr_size_up, hdr_size_lo)
				= in_pkt.range(deref_size_up, deref_size_lo)
				+ sizeof(struct legomem_read_write_req);
			wait_pkt.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			// filled with offset
			wait_pkt.pkt.range(mem_va_up, mem_va_lo)
				= in_pkt.range(deref_off2_up, deref_off2_lo);
			wait_pkt.pkt.range(mem_size_up, mem_size_lo)
				= in_pkt.range(deref_size_up, deref_size_lo);
			wait_pkt.pkt.range(mem_1st_data1_up, mem_1st_data1_lo)
				= in_pkt.range(deref_1st_data_up, deref_1st_data_lo);

			// maintain state machine for large packet
			data_remain = in_pkt.range(hdr_size_up, hdr_size_lo);
			if (data_remain <= DATASIZE) {
				wait_pkt.last = 1;
				data_remain = 0;
				delay_pkt(to_waitqueue_delay, wait_pkt);
			} else {
				data_remain -= DATASIZE;
				state = DATATOWAITQUEUE;
			}

			seqid_wq.push(in_pkt.range(hdr_seqId_up, hdr_seqId_lo));
			break;

		case OP_REQ_READ_RESP:
			if (seqid_wq.front() == in_pkt.range(hdr_seqId_up, hdr_seqId_lo)) {
				// release wait queue
				release.addr = in_pkt.range(mem_ret_addr_up, mem_ret_addr_lo);
				release.status = in_pkt.range(hdr_req_status_up, hdr_req_status_lo);
				delay_pkt(release_delay, release);
				seqid_wq.pop();
			} else {
				// forward packet back to network
				send_pkt.pkt = in_pkt;
				send_pkt.pkt.range(hdr_opcode_up, hdr_opcode_lo) = OP_REQ_IREAD_RESP;
				send_pkt.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_NET;
				data_remain = in_pkt.range(hdr_size_up, hdr_size_lo);
				if (data_remain > DATASIZE) {
					send_pkt.last = 0;
					data_remain -= DATASIZE;
					state = DATATOSENDQUEUE;
				} else {
					send_pkt.last = 1;
				}
				delay_pkt(to_sendqueue_delay, send_pkt);
			}
			break;

		case OP_REQ_WRITE_RESP:
			send_pkt.pkt.range(hdr_up, hdr_lo) = in_pkt.range(hdr_up, hdr_lo);
			send_pkt.pkt.range(hdr_opcode_up, hdr_opcode_lo) = OP_REQ_IWRITE_RESP;
			send_pkt.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_NET;
			send_pkt.last = 1;
			delay_pkt(to_sendqueue_delay, send_pkt);
			break;

		default:
			break;
		}
		break;

	case DATATOWAITQUEUE:
		if (data_in.empty())
			break;
		in_pkt = data_in.read();

		// padding last packet
		/**
		 * because data2 in both mem_rest and mem_1st structure
		 * has same size and same position, we can cast to either of them
		 */
		wait_pkt.pkt.range(mem_rest_data2_up, mem_rest_data2_lo)
			= in_pkt.range(deref_rest_data1_up, deref_rest_data1_lo);
		if (data_remain <= MISMATCHSIZE) {
			wait_pkt.last = 1;
			delay_pkt(to_waitqueue_delay, wait_pkt);

			data_remain = 0;
			state = PARSE;
		} else {
			wait_pkt.last = 0;
			delay_pkt(to_waitqueue_delay, wait_pkt);

			// filled current packet
			wait_pkt.pkt = 0;
			wait_pkt.pkt.range(mem_rest_data1_up, mem_rest_data1_lo)
				= in_pkt.range(deref_rest_data2_up, deref_rest_data2_lo);
			if (data_remain <= DATASIZE) {
				wait_pkt.last = 1;
				data_remain = 0;
				state = DATATOWAITQUEUE2;
			} else {
				wait_pkt.last = 0;
				data_remain -= DATASIZE;
			}
		}
		break;

	case DATATOWAITQUEUE2:
		delay_pkt(to_waitqueue_delay, wait_pkt);
		state = PARSE;
		break;

	case DATATOSENDQUEUE:
		send_pkt.pkt = in_pkt;
		if (data_remain > DATASIZE) {
			data_remain -= DATASIZE;
			send_pkt.last = 0;
		} else {
			state = PARSE;
			send_pkt.last = 1;
		}
		delay_pkt(to_sendqueue_delay, send_pkt);
		break;

	default:
		break;
	}
}

static void
waitqueue(stream<struct data_if> &from_decode,
	  stream<struct data_if> &to_sendqueue,
	  stream<struct release_if> &release)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum wq_fsm {HEADER, DATA};
	static wq_fsm state					= HEADER;
	static ap_uint<1> drop 					= 0;		// drop if error occurs

	// delay packet to sendqueue for one cycle to meet timing
	static struct delay<struct data_if> to_sendqueue_delay	= {0,0};

	struct release_if release_pkt				= {0,0};
	struct data_if waiting_pkt 				= {0,0};
	struct data_if send_pkt					= {0,0};

	if (to_sendqueue_delay.vld) {
		to_sendqueue.write(to_sendqueue_delay.data);
		to_sendqueue_delay.vld = 0;
	}

	switch (state) {
	case HEADER:
		if (release.empty())
			break;
		// queue should not be empty at this point
		waiting_pkt = from_decode.read();
		release_pkt = release.read();

		waiting_pkt.pkt.range(mem_va_up, mem_va_lo)
			= release_pkt.addr + waiting_pkt.pkt.range(mem_va_up, mem_va_lo);
		send_pkt.pkt = waiting_pkt.pkt;
		send_pkt.last = waiting_pkt.last;
		if (waiting_pkt.last == 0)
			state = DATA;
		// status OKAY
		if (release_pkt.status != 0) {
			drop = 1;
		} else {
			drop = 0;
			delay_pkt(to_sendqueue_delay, send_pkt);
		}
		break;

	case DATA:
		if (from_decode.empty())
			break;

		waiting_pkt = from_decode.read();
		send_pkt.pkt = waiting_pkt.pkt;
		send_pkt.last = waiting_pkt.last;
		if (waiting_pkt.last == 1)
			state = HEADER;
		if (drop == 0)
			delay_pkt(to_sendqueue_delay, send_pkt);
		break;

	default:
		break;
	}
}

static void
sendqueue(stream<struct data_if> &from_decode,
	  stream<struct data_if> &from_waitqueue,
	  stream<ap_uint<DATAWIDTH> > &data_out)
{
#pragma HLS PIPELINE
#pragma HLS INLINE off

	enum sendqueue_fsm {INIT, DECODE_CONT, WAITQUEUE_CONT};
	enum Source {NONE, DECODE, WQ};

	static sendqueue_fsm state			= INIT;
	struct data_if out_pkt				= {0,0};
	static Source roundrobin			= DECODE;
	Source decision					= NONE;

	switch (state) {
	case INIT:
		if (!from_decode.empty() && !from_waitqueue.empty())
			decision = roundrobin;
		else if (!from_decode.empty())
			decision = DECODE;
		else if (!from_waitqueue.empty())
			decision = WQ;
		else
			decision = NONE;

		switch (decision) {
		case DECODE:
			out_pkt = from_decode.read();
			if (out_pkt.last == 0)
				state = DECODE_CONT;
			roundrobin = WQ;

			data_out.write(out_pkt.pkt);
			break;

		case WQ:
			out_pkt = from_waitqueue.read();
			if (out_pkt.last == 0)
				state = WAITQUEUE_CONT;
			decision = DECODE;

			data_out.write(out_pkt.pkt);
			break;

		default:
			break;
		}
		break;

	case DECODE_CONT:
		if (from_decode.empty())
			break;

		out_pkt = from_decode.read();
		if (out_pkt.last == 1)
			state = INIT;

		data_out.write(out_pkt.pkt);
		break;

	case WAITQUEUE_CONT:
		if (from_waitqueue.empty())
			break;

		out_pkt = from_waitqueue.read();
		if (out_pkt.last == 1)
			state = INIT;

		data_out.write(out_pkt.pkt);
		break;

	default:
		break;
	}
}

void deref_ptr(hls::stream<ap_uint<DATAWIDTH> > &data_in, hls::stream<ap_uint<DATAWIDTH> > &data_out)
{
// remove ap_ctrl_none before doing cosim test
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE port=data_in axis
#pragma HLS INTERFACE port=data_out axis
#pragma HLS DATAFLOW

	static stream<struct data_if> decode2sendqueue("decode2sendqueue");
	static stream<struct data_if> wq2sendqueue("waitqueue2sendqueue");
	static stream<struct data_if> decode2wq_data("decode2waitqueue_data");
	static stream<struct release_if> decode2wq_release("decode2waitqueue_release");

#pragma HLS DATA_PACK variable=decode2sendqueue
#pragma HLS DATA_PACK variable=wq2sendqueue
#pragma HLS DATA_PACK variable=decode2wq_data
#pragma HLS DATA_PACK variable=decode2wq_release

#pragma HLS STREAM variable=decode2sendqueue	depth=8
#pragma HLS STREAM variable=wq2sendqueue	depth=8
#pragma HLS STREAM variable=decode2wq_data	depth=64
#pragma HLS STREAM variable=decode2wq_release	depth=8

	decode(data_in, decode2sendqueue, decode2wq_data, decode2wq_release);
	waitqueue(decode2wq_data, wq2sendqueue, decode2wq_release);
	sendqueue(decode2sendqueue, wq2sendqueue, data_out);
}

