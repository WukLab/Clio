/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <stdio.h>
#include <vector>
#include "rx_64.hpp"
#define MAX_CYCLE 50

using namespace std;

int cycle = 0;

class test_util {
       public:
	test_util();
	~test_util() {}
	void run_one_cycle(stream<struct udp_info> *rx_header,
			   stream<struct net_axis_64> *rx_payload);

       private:
	stream<struct udp_info> rsp_header;
	stream<struct net_axis_64> rsp_payload;
	stream<struct udp_info> ack_header;
	stream<struct net_axis_64> ack_payload;
	stream<struct udp_info> usr_rx_header;
	stream<struct net_axis_64> usr_rx_payload;
};

test_util::test_util()
    : rsp_header("response ack header"),
      rsp_payload("response ack packet"),
      ack_header("received ack header"),
      ack_payload("received ack packet"),
      usr_rx_header("header to MMU"),
      usr_rx_payload("payload to MMU") {
	cycle = 0;
}

void test_util::run_one_cycle(stream<struct udp_info> *rx_header,
			      stream<struct net_axis_64> *rx_payload)
{
	struct udp_info recv_hd;
	struct net_axis_64 recv_data;
	ap_uint<1> reset;

	reset = (cycle == 0 ? 1 : 0);

	rx_64(rx_header, rx_payload, &rsp_header, &rsp_payload, &ack_header,
	      &ack_payload, &usr_rx_header, &usr_rx_payload, reset);

	if (!rsp_header.empty()) {
		recv_hd = rsp_header.read();
		dph("[cycle %2d] host get response %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}
	if (!rsp_payload.empty()) {
		recv_data = rsp_payload.read();
		ap_uint<8> type = recv_data.data(7, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(8 + SEQ_WIDTH - 1, 8);
		dph("[cycle %2d] host get response: [type %d, seq %lld]\n",
		    cycle, type.to_ushort(), seqnum.to_uint64());
	}
	if (!ack_header.empty()) {
		recv_hd = ack_header.read();
		dph("[cycle %2d] rx receive ack %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}
	if (!ack_payload.empty()) {
		recv_data = ack_payload.read();
		ap_uint<8> type = recv_data.data(7, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(8 + SEQ_WIDTH - 1, 8);
		dph("[cycle %2d] rx received ack: [type %d, seq %lld]\n", cycle,
		    type.to_ushort(), seqnum.to_uint64());
	}
	if (!usr_rx_header.empty()) {
		recv_hd = usr_rx_header.read();
		dph("[cycle %2d] send hdr to MMU %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}
	if (!usr_rx_payload.empty()) {
		recv_data = usr_rx_payload.read();
		dph("[cycle %2d] send data to MMU %llx, ", cycle,
		    recv_data.data.to_uint64());
		ap_uint<8> type = recv_data.data(7, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(8 + SEQ_WIDTH - 1, 8);
		dph("if gbn header [type %d, seq %lld]\n",
		    type.to_ushort(), seqnum.to_uint64());
	}

	cycle++;
}

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

/* test receive data */
void test1(vector<unsigned> &test_seq)
{
	printf("----------test1-----------\n");

	test_util rx_64_util;
	struct udp_info test_header;
	struct net_axis_64 test_payload;
	stream<struct udp_info> rx_header("receive udp header info");
	stream<struct net_axis_64> rx_payload("receive udp packet");

	test_header.src_ip = 0xc0a80181;   // 192.168.1.129
	test_header.dest_ip = 0xc0a80180;  // 192.168.1.128
	test_header.src_port = 1234;
	test_header.dest_port = 2345;

	test_payload.keep = 0xff;
	test_payload.user = 0;

	for (; cycle < MAX_CYCLE;) {
		if (cycle < test_seq.size()) {
			dph("[cycle %2d] host send %x:%d -> %x:%d\n", cycle,
			    test_header.src_ip.to_uint(),
			    test_header.src_port.to_uint(),
			    test_header.dest_ip.to_uint(),
			    test_header.dest_port.to_uint());
			rx_header.write(test_header);

			test_payload = build_gbn_header(pkt_type_data,
							 test_seq[cycle], 0);
			dph("[cycle %2d] host send gbn header [type %d, seq %lld]\n",
			    cycle, test_payload.data(7, 0).to_uint(),
			    test_payload.data(7 + SEQ_WIDTH, 8).to_uint64());

			rx_payload.write(test_payload);
			for (int i = 0; i < 2; i++) {
				test_payload.data = 0x0f0f0f0f0f0f0f0f;
				test_payload.last = 0;
				rx_payload.write(test_payload);
			}
			test_payload.data = 0x01;
			test_payload.last = 1;
			rx_payload.write(test_payload);
		}
		rx_64_util.run_one_cycle(&rx_header, &rx_payload);
	}

	printf("-------test1 done---------\n");
}

/* test receive ack */
void test2(vector<unsigned> &test_seq)
{
	printf("----------test2-----------\n");

	test_util rx_64_util;
	struct udp_info test_header;
	struct net_axis_64 test_payload;
	stream<struct udp_info> rx_header("receive udp header info");
	stream<struct net_axis_64> rx_payload("receive udp packet");

	test_header.src_ip = 0xc0a80181;   // 192.168.1.129
	test_header.dest_ip = 0xc0a80180;  // 192.168.1.128
	test_header.src_port = 1234;
	test_header.dest_port = 2345;

	test_payload.keep = 0xff;
	test_payload.user = 0;

	for (; cycle < MAX_CYCLE;) {
		if (cycle < test_seq.size()) {
			dph("[cycle %2d] host send %x:%d -> %x:%d\n", cycle,
			    test_header.src_ip.to_uint(),
			    test_header.src_port.to_uint(),
			    test_header.dest_ip.to_uint(),
			    test_header.dest_port.to_uint());
			rx_header.write(test_header);

			test_payload =
			    build_gbn_header(pkt_type_ack, test_seq[cycle], 1);
			dph("[cycle %2d] host send ack [type %d, seq %lld]\n",
			    cycle, test_payload.data(7, 0).to_uint(),
			    test_payload.data(7 + SEQ_WIDTH, 8).to_uint64());
			rx_payload.write(test_payload);
		}
		rx_64_util.run_one_cycle(&rx_header, &rx_payload);
	}

	printf("-------test2 done---------\n");
}

int main()
{
	vector<unsigned> seq1 = {1, 2, 3, 4, 5, 6, 7};
	vector<unsigned> seq2 = {1, 2, 3, 5, 6, 7};
	vector<unsigned> seq3 = {1, 2, 3, 5, 6, 7, 4, 8};
	vector<unsigned> seq4 = {1, 2, 3, 5, 6, 2, 3, 4, 5, 6};
	test1(seq1);
	test1(seq2);
	test1(seq3);
	test1(seq4);
	test2(seq1);
	return 0;
}
