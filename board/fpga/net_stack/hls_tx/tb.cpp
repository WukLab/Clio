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
#define MAX_CYCLE 50

using namespace std;

int cycle = 0;

class test_util {
       public:
	test_util();
	~test_util() {}
	void run_one_cycle(stream<struct udp_info> *usr_tx_header,
			   stream<struct net_axis_64> *usr_tx_payload,
			   stream<ap_uint<SEQ_WIDTH> > *check_full_rsp);

       private:
	// out stream channel
	stream<struct udp_info> tx_header;
	stream<struct net_axis_64> tx_payload;
	stream<ap_uint<SLOT_ID_WIDTH> > check_full_req;
	stream<bool> tx_finish_sig;
	stream<struct net_axis_64> tx_buff_payload;
	stream<struct route_info> tx_buff_route_info;
};

test_util::test_util()
    : tx_header("header to network"),
      tx_payload("payload to network"),
      check_full_req("tx check buffer full"),
      tx_finish_sig("tx completion signal"),
      tx_buff_payload("payload to buff"),
      tx_buff_route_info("route info to buff") {
	cycle = 0;
}

void test_util::run_one_cycle(stream<struct udp_info> *usr_tx_header,
			      stream<struct net_axis_64> *usr_tx_payload,
			      stream<ap_uint<SEQ_WIDTH> > *check_full_rsp)
{
	struct udp_info recv_hd;
	struct net_axis_64 recv_data;

	tx_64(&tx_header, &tx_payload,
	      usr_tx_header, usr_tx_payload,
	      &check_full_req, check_full_rsp,
	      &tx_finish_sig, &tx_buff_payload, &tx_buff_route_info);

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
		ap_uint<8> type = recv_data.data(PKT_TYPE_WIDTH - 1, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		ap_uint<SLOT_ID_WIDTH> src_slot = recv_data.data(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		ap_uint<SES_ID_WIDTH> dest_slot = recv_data.data(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		dph("if gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
		    type.to_ushort(), seqnum.to_uint64(), src_slot.to_uint(),
		    dest_slot.to_uint());
	}
	if (!check_full_req.empty()) {
		ap_uint<SLOT_ID_WIDTH> slotid = check_full_req.read();
		dph("[cycle %2d] state table get check full request, slot %d\n",
		    cycle, slotid.to_uint());
	}
	if (!tx_finish_sig.empty()) {
		bool sig = tx_finish_sig.read();
		dph("[cycle %2d] state table get tx completion signal\n", cycle);
	}
	if (!tx_buff_payload.empty()) {
		recv_data = tx_buff_payload.read();
		ap_uint<8> type = recv_data.data(PKT_TYPE_WIDTH - 1, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		ap_uint<SLOT_ID_WIDTH> src_slot = recv_data.data(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		ap_uint<SLOT_ID_WIDTH> dest_slot = recv_data.data(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		dph("[cycle %2d] tx buffer get data: %llx, ",
		    cycle, recv_data.data.to_uint64());
		dph("if gbn header, [type %d, seq %lld, src slot %d, dest slot %d]\n",
		    type.to_ushort(), seqnum.to_uint64(), src_slot.to_uint(),
		    dest_slot.to_uint());
	}
	if (!tx_buff_route_info.empty()) {
		struct route_info recv_ri = tx_buff_route_info.read();
		ap_uint<32> src_ip = recv_ri.ip_info.src_ip;
		ap_uint<32> dest_ip = recv_ri.ip_info.dest_ip;
		dph("[cycle %2d] tx buffer get route info: %lx -> %lx\n", cycle,
		    src_ip.to_uint(), dest_ip.to_uint());
	}

	cycle++;
}

/* test receive ack */
void test_data()
{
	printf("----------test data-----------\n");

	test_util tx_64_util;
	vector<unsigned> send_seq = {1, 2, 3, 4, 5, 6, 7, 8};

	stream<struct udp_info> usr_tx_header;
	stream<struct net_axis_64> usr_tx_payload;
	stream<ap_uint<SEQ_WIDTH> > check_full_rsp;
	struct udp_info test_header;
	struct net_axis_64 test_payload;

	test_header.src_ip = 0xc0a80180;   // 192.168.1.128
	test_header.dest_ip = 0xc0a80102;  // 192.168.1.2
	test_header.src_port = 10;
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
			test_payload.data = send_seq[i];
			test_payload.last = 1;
			usr_tx_payload.write(test_payload);
		}
		tx_64_util.run_one_cycle(&usr_tx_header, &usr_tx_payload,
					 &check_full_rsp);
	}

	for (int i = 0; cycle < 2 * MAX_CYCLE; i++) {
		if (i < send_seq.size()) {
			check_full_rsp.write(send_seq[i]);
		}
		tx_64_util.run_one_cycle(&usr_tx_header, &usr_tx_payload,
					 &check_full_rsp);
	}

	printf("-------test data done---------\n");
}

int main() {
	test_data();
	return 0;
}
