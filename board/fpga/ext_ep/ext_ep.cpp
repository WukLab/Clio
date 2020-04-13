/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "ext_ep.h"

using namespace hls;

static void
decode(stream<ap_uint<DATAWIDTH> > &data_in,
       stream<struct queue_entry> &to_sendqueue,
       stream<struct queue_entry> &to_waitqueue,
       stream<struct queue_release> &waitqueue_release)
{
	enum decode_fsm {PARSE, DATATOWAITQUEUE, DATATOSENDQUEUE};

	// stateful variable, remain the same between cycles
	static WaitQueue seqIdqueue;
	static ap_uint<16> data_remain			= 0;
	static decode_fsm state				= PARSE;
	static struct queue_entry entry			= {0,0};

	// temporary variable
	ap_uint<DATAWIDTH> in_packet			= 0;
	struct queue_entry out_packet			= {0,0};
	struct queue_release release			= {0,0};

	switch (state) {
	case PARSE:
		if (data_in.empty())
			break;
		in_packet = data_in.read();

		switch (((struct lego_mem_header *)&in_packet)->req_type) {
		case LEGOMEM_REQ_IREAD:
			entry.pkt = 0;
			entry.last = 0;

			// for deref request, first half request to core mem
			out_packet.pkt.range(hdr_up, hdr_lo) = in_packet.range(hdr_up, hdr_lo);
			out_packet.pkt.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_READ;
			out_packet.pkt.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_read_write_req);
			out_packet.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			out_packet.pkt.range(mem_va_up, mem_va_lo)
				= in_packet.range(deref_addr_up, deref_addr_lo)
				+ in_packet.range(deref_off1_up, deref_off1_lo);
			out_packet.pkt.range(mem_size_up, mem_size_lo) = sizeof(void *);
			out_packet.last = 1;
			to_sendqueue.write(out_packet);

			// second half to wait
			entry.pkt.range(hdr_up, hdr_lo) = in_packet.range(hdr_up, hdr_lo);
			entry.pkt.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_READ;
			entry.pkt.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_read_write_req);
			entry.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			// filled with offset
			entry.pkt.range(mem_va_up, mem_va_lo)
				= in_packet.range(deref_off2_up, deref_off2_lo);
			entry.pkt.range(mem_size_up, mem_size_lo)
				= in_packet.range(deref_size_up, deref_size_lo);
			entry.last = 1;
			to_waitqueue.write(entry);

			// record waiting sequence ID
			seqIdqueue.push(in_packet.range(hdr_seqId_up, hdr_seqId_lo));
			break;

		case LEGOMEM_REQ_IWRITE:
			entry.pkt = 0;
			entry.last = 0;

			// for deref request, first half request to core mem
			out_packet.pkt.range(hdr_up, hdr_lo) = in_packet.range(hdr_up, hdr_lo);
			out_packet.pkt.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_READ;
			out_packet.pkt.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_read_write_req);
			out_packet.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			out_packet.pkt.range(mem_va_up, mem_va_lo)
				= in_packet.range(deref_addr_up, deref_addr_lo)
				+ in_packet.range(deref_off1_up, deref_off1_lo);
			out_packet.pkt.range(mem_size_up, mem_size_lo) = sizeof(void *);
			out_packet.last = 1;
			to_sendqueue.write(out_packet);

			// second half to wait
			entry.pkt.range(hdr_up, hdr_lo) = in_packet.range(hdr_up, hdr_lo);
			entry.pkt.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_WRITE;
			entry.pkt.range(hdr_size_up, hdr_size_lo)
				= in_packet.range(deref_size_up, deref_size_lo)
				+ sizeof(struct legomem_read_write_req);
			entry.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_MEM;
			// filled with offset
			entry.pkt.range(mem_va_up, mem_va_lo)
				= in_packet.range(deref_off2_up, deref_off2_lo);
			entry.pkt.range(mem_size_up, mem_size_lo)
				= in_packet.range(deref_size_up, deref_size_lo);
			entry.pkt.range(mem_1st_data1_up, mem_1st_data1_lo)
				= in_packet.range(deref_1st_data_up, deref_1st_data_lo);

			// maintain state machine for large packet
			data_remain = in_packet.range(hdr_size_up, hdr_size_lo);
			if (data_remain <= DATASIZE) {
				entry.last = 1;
				data_remain = 0;
				to_waitqueue.write(entry);
			} else {
				data_remain -= DATASIZE;
				state = DATATOWAITQUEUE;
			}

			// record waiting sequence ID
			seqIdqueue.push(in_packet.range(hdr_seqId_up, hdr_seqId_lo));
			break;

		case LEGOMEM_REQ_READ_RESP:
			if (!seqIdqueue.empty() && seqIdqueue.front() == in_packet.range(hdr_seqId_up, hdr_seqId_lo)) {
				// release wait queue
				release.addr = in_packet.range(mem_ret_addr_up, mem_ret_addr_lo);
				release.status = in_packet.range(hdr_req_status_up, hdr_req_status_lo);
				waitqueue_release.write(release);
				seqIdqueue.pop();
			} else {
				// forward packet back to network
				out_packet.pkt = in_packet;
				out_packet.pkt.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_IREAD_RESP;
				out_packet.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_NET;
				data_remain = in_packet.range(hdr_size_up, hdr_size_lo);
				if (data_remain > DATASIZE) {
					data_remain -= DATASIZE;
					state = DATATOSENDQUEUE;
					out_packet.last = 0;
				} else {
					out_packet.last = 1;
				}
				to_sendqueue.write(out_packet);
			}
			break;

		case LEGOMEM_REQ_WRITE_RESP:
			out_packet.pkt.range(hdr_up, hdr_lo) = in_packet.range(hdr_up, hdr_lo);
			out_packet.pkt.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_IWRITE_RESP;
			out_packet.pkt.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_NET;
			out_packet.last = 1;
			to_sendqueue.write(out_packet);
			break;

		default:
			break;
		}
		break;

	case DATATOWAITQUEUE:
		if (data_in.empty())
			break;
		in_packet = data_in.read();

		// padding last packet
		/**
		 * because data2 in both mem_rest and mem_1st structure
		 * has same size and same position, we can cast to either of them
		 */
		entry.pkt.range(mem_rest_data2_up, mem_rest_data2_lo)
			= in_packet.range(deref_rest_data1_up, deref_rest_data1_lo);
		if (data_remain <= MISMATCHSIZE) {
			entry.last = 1;
			data_remain = 0;
			to_waitqueue.write(entry);
			break;
		}
		entry.last = 0;
		to_waitqueue.write(entry);

		// filled current packet
		entry.pkt = 0;
		entry.last = 0;
		entry.pkt.range(mem_rest_data1_up, mem_rest_data1_lo)
			= in_packet.range(deref_rest_data2_up, deref_rest_data2_lo);
		if (data_remain <= DATASIZE) {
			entry.last = 1;
			data_remain = 0;
			to_waitqueue.write(entry);
			state = PARSE;
		} else {
			data_remain -= DATASIZE;
		}
		break;

	case DATATOSENDQUEUE:
		out_packet.pkt = in_packet;
		if (data_remain > DATASIZE) {
			data_remain -= DATASIZE;
			out_packet.last = 0;
		} else {
			state = PARSE;
			out_packet.last = 1;
		}
		to_sendqueue.write(out_packet);
		break;

	default:
		break;
	}
}

static void
waitqueue(stream<struct queue_entry> &from_decode,
	  stream<struct queue_entry> &to_sendqueue,
	  stream<struct queue_release> &release)
{
	enum waitqueue_fsm {HEADER, DATA};

	static stream<struct queue_entry> queue;
	static waitqueue_fsm state		= HEADER;
	static ap_uint<1> drop 			= 0;		// drop if error occurs
	static ap_uint<9> seqid			= 0;

	struct queue_release a_release 		= {0,0};
	struct queue_entry entry 		= {0,0};
	struct queue_entry out_packet		= {0,0};

#pragma HLS STREAM variable=queue depth=128

	switch (state) {
	case HEADER:
		if (release.empty())
			break;
		// queue should not be empty at this point
		a_release = release.read();
		entry = queue.read();

		entry.pkt.range(mem_va_up, mem_va_lo)
			= a_release.addr + entry.pkt.range(mem_va_up, mem_va_lo);
		out_packet.pkt = entry.pkt;
		out_packet.last = entry.last;
		if (entry.last == 0)
			state = DATA;
		if (a_release.status != LEGOMEM_STATUS_OKAY) {
			drop = 1;
		} else {
			drop = 0;
			to_sendqueue.write(out_packet);
		}
		break;

	case DATA:
		if (queue.empty())
			break;

		entry = queue.read();
		out_packet.pkt = entry.pkt;
		out_packet.last = entry.last;
		if (entry.last == 1)
			state = HEADER;
		if (drop == 0)
			to_sendqueue.write(out_packet);
		break;

	default:
		break;
	}

	if (!from_decode.empty())
		queue.write(from_decode.read());

}

static void
sendqueue(stream<struct queue_entry> &from_decode,
	  stream<struct queue_entry> &from_waitqueue,
	  stream<ap_uint<DATAWIDTH> > &data_out)
{
	enum sendqueue_fsm {INIT, DECODE_CONT, WAITQUEUE_CONT};
	enum Source {NONE, DECODE, WQ};

	static sendqueue_fsm state	= INIT;
	struct queue_entry packet	= {0,0};
	static Source roundrobin	= DECODE;
	Source decision			= NONE;

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
			packet = from_decode.read();
			if (packet.last == 0)
				state = DECODE_CONT;
			data_out.write(packet.pkt);
			roundrobin = WQ;
			break;

		case WQ:
			packet = from_waitqueue.read();
			if (packet.last == 0)
				state = WAITQUEUE_CONT;
			data_out.write(packet.pkt);
			decision = DECODE;
			break;

		default:
			break;
		}
		break;

	case DECODE_CONT:
		if (from_decode.empty())
			break;

		packet = from_decode.read();
		if (packet.last == 1)
			state = INIT;
		data_out.write(packet.pkt);
		break;

	case WAITQUEUE_CONT:
		if (from_waitqueue.empty())
			break;

		packet = from_waitqueue.read();
		if (packet.last == 1)
			state = INIT;
		data_out.write(packet.pkt);
		break;

	default:
		break;
	}
}

void ext_ep(hls::stream<ap_uint<DATAWIDTH> > &data_in, hls::stream<ap_uint<DATAWIDTH> > &data_out)
{
// remove ap_ctrl_none before doing cosim test
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE port=data_in axis
#pragma HLS INTERFACE port=data_out axis
#pragma HLS DATAFLOW

	static stream<struct queue_entry> decode2sendqueue("decode2sendqueue");
	static stream<struct queue_entry> wq2sendqueue("waitqueue2sendqueue");
	static stream<struct queue_entry> decode2wq_data("decode2waitqueue_data");
	static stream<struct queue_release> decode2wq_release("decode2waitqueue_release");

#pragma HLS DATA_PACK variable=decode2sendqueue
#pragma HLS DATA_PACK variable=wq2sendqueue
#pragma HLS DATA_PACK variable=decode2wq_data
#pragma HLS DATA_PACK variable=decode2wq_release

#pragma HLS STREAM variable=decode2sendqueue 		depth=4
#pragma HLS STREAM variable=wq2sendqueue		depth=4
#pragma HLS STREAM variable=decode2wq_data		depth=4
#pragma HLS STREAM variable=decode2wq_release 		depth=4

	decode(data_in, decode2sendqueue, decode2wq_data, decode2wq_release);
	waitqueue(decode2wq_data, wq2sendqueue, decode2wq_release);
	sendqueue(decode2sendqueue, wq2sendqueue, data_out);
}

