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
			   stream<struct net_axis_64> *rx_payload,
			   stream<bool> *state_query_rsp);

    private:
	stream<struct query_req> state_query_req;
	stream<struct udp_info> usr_rx_header;
	stream<struct net_axis_64> usr_rx_payload;
};

test_util::test_util()
    : state_query_req("rx state query request"),
      usr_rx_header("header to MMU"),
      usr_rx_payload("payload to MMU") {
	cycle = 0;
}

void test_util::run_one_cycle(stream<struct udp_info> *rx_header,
			      stream<struct net_axis_64> *rx_payload,
			      stream<bool> *state_query_rsp)
{
	struct udp_info recv_hd;
	struct net_axis_64 recv_data;

	rx_64(rx_header, rx_payload, &state_query_req, state_query_rsp,
	      &usr_rx_header, &usr_rx_payload);

	if (!state_query_req.empty()) {
		struct query_req gbn_query = state_query_req.read();
		ap_uint<8> type = gbn_query.gbn_header(PKT_TYPE_WIDTH - 1, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    gbn_query.gbn_header(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		ap_uint<SLOT_ID_WIDTH> src_slot = gbn_query.gbn_header(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		ap_uint<SLOT_ID_WIDTH> dest_slot = gbn_query.gbn_header(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		dph("[cycle %2d] state tabel get gbn header: [type %d, seq %lld, src slot %d, dest slot %d]\n",
		    cycle, type.to_ushort(), seqnum.to_uint64(),
		    src_slot.to_uint(), dest_slot.to_uint());
	}
	if (!usr_rx_header.empty()) {
		recv_hd = usr_rx_header.read();
		dph("[cycle %2d] send hdr to MMU %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}
	if (!usr_rx_payload.empty()) {
		recv_data = usr_rx_payload.read();
		dph("[cycle %2d] send data to MMU %llx\n", cycle,
		    recv_data.data.to_uint64());
		ap_uint<8> type = recv_data.data(7, 0);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(8 + SEQ_WIDTH - 1, 8);
	}

	cycle++;
}

ap_uint<SES_ID_WIDTH> build_sesid(short src_slot, short dest_slot)
{
	ap_uint<SES_ID_WIDTH> ses_id;
	ses_id(SLOT_ID_WIDTH - 1, 0) = src_slot;
	ses_id(2 * SLOT_ID_WIDTH - 1, SLOT_ID_WIDTH) = dest_slot;
	ses_id(SES_ID_WIDTH - 1, 2 * SLOT_ID_WIDTH) = 0;
	return ses_id;
}

struct net_axis_64 build_gbn_header(ap_uint<SES_ID_WIDTH> ses_id,
				    ap_uint<PKT_TYPE_WIDTH> type,
				    ap_uint<SEQ_WIDTH> seqnum, ap_uint<1> last)
{
	struct net_axis_64 pkt;
	pkt.data(PKT_TYPE_WIDTH - 1, 0) = type;
	pkt.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET) = seqnum;
	pkt.data(SES_ID_OFFSET + SES_ID_WIDTH - 1, SES_ID_OFFSET) = ses_id;
	pkt.keep = 0xff;
	pkt.last = last;
	pkt.user = 0;
	return pkt;
}

/* test receive data */
void test_data(vector<unsigned> &test_seq)
{
	printf("----------test data-----------\n");

	test_util rx_64_util;
	struct udp_info test_header;
	struct net_axis_64 test_payload;
	ap_uint<SES_ID_WIDTH> ses_id;
	stream<struct udp_info> rx_header("receive udp header info");
	stream<struct net_axis_64> rx_payload("receive udp packet");
	stream<bool> state_query_rsp("query response");

	test_header.src_ip = 0xc0a80102;   // 192.168.1.2
	test_header.dest_ip = 0xc0a80180;  // 192.168.1.128
	test_header.src_port = 1234;
	test_header.dest_port = 2345;

	test_payload.keep = 0xff;
	test_payload.user = 0;

	for (int i = 0; cycle < MAX_CYCLE; i++) {
		if (cycle < test_seq.size()) {
			dph("[cycle %2d] host send %x:%d -> %x:%d\n", cycle,
			    test_header.src_ip.to_uint(),
			    test_header.src_port.to_uint(),
			    test_header.dest_ip.to_uint(),
			    test_header.dest_port.to_uint());
			rx_header.write(test_header);

			ses_id = build_sesid(20, 10);
			test_payload = build_gbn_header(ses_id, GBN_PKT_DATA,
							test_seq[i], 0);
			dph("[cycle %2d] host send gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
			    cycle, test_payload.data(7, 0).to_uint(),
			    test_payload.data(7 + SEQ_WIDTH, 8).to_uint64(),
			    ses_id(SLOT_ID_WIDTH - 1, 0).to_uint(),
			    ses_id(2 * SLOT_ID_WIDTH - 1, SLOT_ID_WIDTH).to_uint());

			state_query_rsp.write(i%2);

			rx_payload.write(test_payload);
			for (int j = 0; j < 2; j++) {
				test_payload.data = 0x0f0f0f0f0f0f0f0f;
				test_payload.last = 0;
				rx_payload.write(test_payload);
			}
			test_payload.data = test_seq[i];
			test_payload.last = 1;
			rx_payload.write(test_payload);
		}
		rx_64_util.run_one_cycle(&rx_header, &rx_payload, &state_query_rsp);
	}

	printf("-------test data done---------\n");
}

int main()
{
	vector<unsigned> seq1 = {1, 2, 3, 4, 5, 6, 7};
	test_data(seq1);
	return 0;
}
