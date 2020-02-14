/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

/**
 * define DEBUG_MODE in fpga/kernel.h to run the test
 */

#include <stdio.h>
#include <vector>
#include <utility>
#include "tx_64.hpp"
#include "../hls_queue/queue_64.hpp"
#define MAX_CYCLE 50

using namespace std;

int cycle = 0;

class test_util {
       public:
	test_util();
	~test_util() {}
	void run_one_cycle(stream<struct udp_info> *usr_tx_header,
			   stream<struct net_axis_64> *usr_tx_payload,
			   stream<struct udp_info> *ack_header,
			   stream<struct net_axis_64> *ack_payload);

       private:
	// out stream channel
	stream<struct udp_info> tx_header;
	stream<struct net_axis_64> tx_payload;
	stream<struct udp_info> rt_header;
	stream<struct net_axis_64> rt_payload;
	// stream channel between sender and queue
	stream<struct bram_cmd> rd_cmd;
	stream<struct bram_cmd> wr_cmd;
	stream<struct net_axis_64> rd_data;
	stream<struct net_axis_64> wr_data;
};

test_util::test_util()
    : tx_header("header to network"),
      tx_payload("payload to network"),
      rt_header("rettrans header to network"),
      rt_payload("retrans payload to net"),
      rd_cmd("read command to queue"),
      wr_cmd("write command to queue"),
      rd_data("read data from queue"),
      wr_data("write data to queue") {
	cycle = 0;
}

void test_util::run_one_cycle(stream<struct udp_info> *usr_tx_header,
			      stream<struct net_axis_64> *usr_tx_payload,
			      stream<struct udp_info> *ack_header,
			      stream<struct net_axis_64> *ack_payload) {
	struct udp_info recv_hd;
	struct net_axis_64 recv_data;

	ap_uint<1> reset;

	reset = (cycle == 0 ? 1 : 0);

	tx_64(&tx_header, &tx_payload,
	      usr_tx_header, usr_tx_payload,
	      ack_header, ack_payload,
	      &rd_cmd, &wr_cmd, &wr_data, &rd_data,
	      &rt_header, &rt_payload, reset);

	queue_64(&rd_cmd, &wr_cmd, &rd_data, &wr_data);

	if (!tx_header.empty()) {
		recv_hd = tx_header.read();
		dph("[cycle %2d] send data to net %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}

	if (!tx_payload.empty()) {
		recv_data = tx_payload.read();
		dph("[cycle %2d] send data to net %llx, ", cycle,
		    recv_data.data.to_uint64());
		ap_uint<8> type = recv_data.data(7, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(8 + SEQ_WIDTH - 1, 8);
		dph("if gbn header [type %d, seq %lld]\n",
		    type.to_ushort(), seqnum.to_uint64());
	}

	if (!rt_header.empty()) {
		recv_hd = rt_header.read();
		dph("[cycle %2d] retransmit data to net %x:%d -> %x:%d\n",
		    cycle, recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}

	if (!rt_payload.empty()) {
		recv_data = rt_payload.read();
		dph("[cycle %2d] retransmit data to net %llx, ", cycle,
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

/* test receive ack */
void test1(vector<unsigned> &ack_seq, vector<enum pkt_type> &ack_type)
{
	printf("----------test1-----------\n");

	test_util tx_64_util;
	vector<unsigned> send_seq = {1, 2, 3, 4, 5, 6, 7, 8};

	stream<struct udp_info> usr_tx_header, ack_header;
	stream<struct net_axis_64> usr_tx_payload, ack_payload;
	struct udp_info test_header;
	struct net_axis_64 test_payload;

	test_header.src_ip = 0xc0a80181;   // 192.168.1.129
	test_header.dest_ip = 0xc0a80180;  // 192.168.1.128
	test_header.src_port = 1234;
	test_header.dest_port = 2345;

	test_payload.keep = 0xff;
	test_payload.user = 0;

	for (int i = 0; cycle < MAX_CYCLE; i++) {
		if (i < send_seq.size()) {
			dph("[cycle %2d] MMU send %x:%d -> %x:%d\n", cycle,
			    test_header.src_ip.to_uint(),
			    test_header.src_port.to_uint(),
			    test_header.dest_ip.to_uint(),
			    test_header.dest_port.to_uint());
			usr_tx_header.write(test_header);

			for (int j = 0; j < 2; j++) {
				test_payload.data = 0x0f0f0f0f0f0f0f0f;
				test_payload.last = 0;
				usr_tx_payload.write(test_payload);
			}
			test_payload.data = 0x01;
			test_payload.last = 1;
			usr_tx_payload.write(test_payload);
		}
		tx_64_util.run_one_cycle(&usr_tx_header, &usr_tx_payload,
					 &ack_header, &ack_payload);
	}

	for (int i = 0; cycle < 2 * MAX_CYCLE; i++) {
		if (i < ack_seq.size()) {
			dph("[cycle %2d] send ack to tx %x:%d -> %x:%d\n", cycle,
			    test_header.src_ip.to_uint(),
			    test_header.src_port.to_uint(),
			    test_header.dest_ip.to_uint(),
			    test_header.dest_port.to_uint());
			ack_header.write(test_header);

			test_payload =
			    build_gbn_header(ack_type[i], ack_seq[i], 1);
			dph("[cycle %2d] send ack to tx: [type %d, seq %lld]\n",
			    cycle, test_payload.data(7, 0).to_uint(),
			    test_payload.data(7 + SEQ_WIDTH, 8).to_uint64());
			ack_payload.write(test_payload);
		}
		tx_64_util.run_one_cycle(&usr_tx_header, &usr_tx_payload,
					 &ack_header, &ack_payload);
	}

	printf("-------test1 done---------\n");
}

/* test time out */
void test2()
{
	printf("----------test2-----------\n");

	test_util tx_64_util;
	vector<unsigned> send_seq = {1, 2, 3, 4, 5, 6, 7, 8};

	stream<struct udp_info> usr_tx_header, ack_header;
	stream<struct net_axis_64> usr_tx_payload, ack_payload;
	struct udp_info test_header;
	struct net_axis_64 test_payload;

	test_header.src_ip = 0xc0a80181;   // 192.168.1.129
	test_header.dest_ip = 0xc0a80180;  // 192.168.1.128
	test_header.src_port = 1234;
	test_header.dest_port = 2345;

	test_payload.keep = 0xff;
	test_payload.user = 0;

	for (int i = 0; cycle < MAX_CYCLE; i++) {
		if (i < send_seq.size()) {
			dph("[cycle %2d] MMU send %x:%d -> %x:%d\n", cycle,
			    test_header.src_ip.to_uint(),
			    test_header.src_port.to_uint(),
			    test_header.dest_ip.to_uint(),
			    test_header.dest_port.to_uint());
			usr_tx_header.write(test_header);

			for (int j = 0; j < 2; j++) {
				test_payload.data = 0x0f0f0f0f0f0f0f0f;
				test_payload.last = 0;
				usr_tx_payload.write(test_payload);
			}
			test_payload.data = 0x01;
			test_payload.last = 1;
			usr_tx_payload.write(test_payload);
		}
		tx_64_util.run_one_cycle(&usr_tx_header, &usr_tx_payload,
					 &ack_header, &ack_payload);
	}

	while (cycle < 2 * MAX_CYCLE + TIMEOUT)
		tx_64_util.run_one_cycle(&usr_tx_header, &usr_tx_payload,
					 &ack_header, &ack_payload);

	printf("-------test2 done---------\n");
}

int main() {
	vector<unsigned> seq1 = {1, 2, 3, 4, 5, 6, 7};
	vector<enum pkt_type> type1(7, pkt_type_ack);
	vector<unsigned> seq2 = {1, 2, 3, 3, 6, 8};
	vector<enum pkt_type> type2 = {pkt_type_ack, pkt_type_ack,
				       pkt_type_ack, pkt_type_nack,
				       pkt_type_ack, pkt_type_ack};
	//test1(seq1, type1);
	//test1(seq2, type2);
	test2();
	return 0;
}
