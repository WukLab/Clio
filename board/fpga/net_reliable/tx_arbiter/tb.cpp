/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "arbiter_64.hpp"
#include <uapi/gbn.h>
#define MAX_CYCLE 50

struct net_axis_64 build_gbn_header(ap_uint<8> type, ap_uint<SEQ_WIDTH> seqnum,
				     ap_uint<1> last)
{
	struct net_axis_64 pkt;
	pkt.data(7, 0) = type;
	pkt.data(8 + SEQ_WIDTH - 1, 8) = seqnum;
	pkt.data(63, 8 + SEQ_WIDTH) = 0;
	pkt.keep = 0xff;
	pkt.last = last;
	pkt.user = 0;
	return pkt;
}

int main() {
	stream<struct udp_info> rsp_header, rt_header, tx_header, out_header;
	stream<struct net_axis_64> rsp_payload, rt_payload, tx_payload,
	    out_payload;

	struct udp_info test_header;
	struct net_axis_64 test_payload;

	test_header.src_ip = 0xc0a80181;   // 192.168.1.129
	test_header.dest_ip = 0xc0a80180;  // 192.168.1.128
	test_header.src_port = 1234;
	test_header.dest_port = 2345;

	rsp_header.write(test_header);
	test_payload = build_gbn_header(GBN_PKT_ACK, 1, 1);
	rsp_payload.write(test_payload);

	test_payload.keep = 0xff;
	test_payload.user = 0;

	rt_header.write(test_header);
	for (int j = 0; j < 10; j++) {
		test_payload.data = 0x0f0f0f0f0f0f0f0f;
		test_payload.last = 0;
		rt_payload.write(test_payload);
	}
	test_payload.data = 0x0f0f0f0f0f0f0f0f;
	test_payload.last = 1;
	rt_payload.write(test_payload);

	tx_header.write(test_header);
	for (int j = 0; j < 10; j++) {
		test_payload.data = 0x0101010101010101;
		test_payload.last = 0;
		tx_payload.write(test_payload);
	}
	test_payload.data = 0x0101010101010101;
	test_payload.last = 1;
	tx_payload.write(test_payload);

	for (int cycle = 0; cycle < MAX_CYCLE; cycle++) {
		arbiter_64(&rsp_header, &rsp_payload, &tx_header, &tx_payload,
			   &rt_header, &rt_payload, &out_header, &out_payload);
		
		struct udp_info recv_hd;
		struct net_axis_64 recv_data;

		if (out_header.read_nb(recv_hd)) {
			dph("[cycle %2d] send data to net %x:%d -> %x:%d\n",
			    cycle, recv_hd.src_ip.to_uint(),
			    recv_hd.src_port.to_uint(),
			    recv_hd.dest_ip.to_uint(),
			    recv_hd.dest_port.to_uint());
		}

		if (out_payload.read_nb(recv_data)) {
			dph("[cycle %2d] send data to net %llx, ", cycle,
			    recv_data.data.to_uint64());
			ap_uint<8> type = recv_data.data(7, 0);
			ap_uint<SEQ_WIDTH> seqnum =
			    recv_data.data(8 + SEQ_WIDTH - 1, 8);
			dph("if gbn header [type %d, seq %lld]\n",
			    type.to_ushort(), seqnum.to_uint64());
		}
	}
}