/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <stdio.h>
#include <vector>
#include "statetable_64.hpp"
#define MAX_CYCLE 100

using namespace std;

int cycle = 0;

class test_util {
    public:
	test_util();
	~test_util() {}
	void run_one_cycle(stream<struct query_req>		*state_query_req,			   
			   stream<ap_uint<SLOT_ID_WIDTH> >	*check_full_req,			   
			   stream<bool>				*tx_finish_sig,
			   stream<ap_uint<SLOT_ID_WIDTH> >	*rt_timeout_sig,
			   stream<struct conn_mgmt_req>		*init_req);

    private:
	stream<struct udp_info> rsp_header;
	stream<struct net_axis_64> rsp_payload;
	stream<struct retrans_req> gbn_retrans_req;
	stream<struct timer_req> timer_rst_req;
	stream<bool> state_query_rsp;
	stream<ap_uint<SEQ_WIDTH> > check_full_rsp;
};

test_util::test_util()
	: rsp_header("response ack header"),
	  rsp_payload("response ack packet"),
	  state_query_rsp("response state query"),
	  gbn_retrans_req("retrans request"),
	  timer_rst_req("reset timer request"),
	  check_full_rsp("response tx full check")
{
	cycle = 0;
}

void test_util::run_one_cycle(stream<struct query_req>		*state_query_req,
			      stream<ap_uint<SLOT_ID_WIDTH> >	*check_full_req,
			      stream<bool>			*tx_finish_sig,
			      stream<ap_uint<SLOT_ID_WIDTH> >	*rt_timeout_sig,
			      stream<struct conn_mgmt_req>	*init_req)
{
	struct udp_info recv_hd;
	struct net_axis_64 recv_data;

	state_table_64(&rsp_header, &rsp_payload,
		       state_query_req, &state_query_rsp,
		       &gbn_retrans_req, &timer_rst_req,
		       rt_timeout_sig, tx_finish_sig,
		       check_full_req, &check_full_rsp, init_req);

	if (!rsp_header.empty()) {
		recv_hd = rsp_header.read();
		dph("[cycle %2d] host get response %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}
	if (!rsp_payload.empty()) {
		recv_data = rsp_payload.read();
		ap_uint<8> type = recv_data.data(PKT_TYPE_OFFSET + PKT_TYPE_WIDTH - 1, PKT_TYPE_OFFSET);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		ap_uint<SLOT_ID_WIDTH> src_slot = recv_data.data(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		ap_uint<SLOT_ID_WIDTH> dest_slot = recv_data.data(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		dph("[cycle %2d] host get response: [type %d, seq %lld, src slot %d, dest slot %d]\n",
		    cycle, type.to_ushort(), seqnum.to_uint64(),
		    src_slot.to_uint(), dest_slot.to_uint());
	}
	if (!state_query_rsp.empty()) {
		bool deliever = state_query_rsp.read();
		dph("[cycle %2d] rx engine get response: if deliever %d\n", cycle,
		    deliever);
	}
	if (!gbn_retrans_req.empty()) {
		struct retrans_req rt_req = gbn_retrans_req.read();
		ap_uint<SLOT_ID_WIDTH> slot = rt_req.slotid;
		ap_uint<SEQ_WIDTH> start = rt_req.seq_start;
		ap_uint<SEQ_WIDTH> end = rt_req.seq_end;
		dph("[cycle %2d] buffer get retrans request: [start %d, end %d, slot %d]\n",
		    cycle, start.to_uint(), end.to_uint(), slot.to_uint());
	}
	if (!timer_rst_req.empty()) {
		struct timer_req timer_rst = timer_rst_req.read();
		ap_uint<8> type = timer_rst.rst_type;
		ap_uint<SLOT_ID_WIDTH> slot = timer_rst.slotid;
		dph("[cycle %2d] timer get request: [type %d, slot %d]\n",
		    cycle, type.to_ushort(), slot.to_uint());
	}
	if (!check_full_rsp.empty()) {
		ap_uint<SEQ_WIDTH> sent_seq = check_full_rsp.read();
		dph("[cycle %2d] tx engine get response: send seq# %d\n", cycle,
		    sent_seq.to_uint());
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
	pkt.data(PKT_TYPE_OFFSET + PKT_TYPE_WIDTH - 1, PKT_TYPE_OFFSET) = type;
	pkt.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET) = seqnum;
	pkt.data(SES_ID_OFFSET + SES_ID_WIDTH - 1, SES_ID_OFFSET) = ses_id;
	pkt.keep = 0xff;
	pkt.last = last;
	pkt.user = 0;
	return pkt;
}

void test_rx(vector<unsigned> &test_seq)
{
	printf("----------test rx-----------\n");
	
	test_util test_rx_util;
	struct query_req test_query;
	struct conn_mgmt_req init;
	ap_uint<SES_ID_WIDTH> ses_id;
	stream<struct query_req> rx_query("receive query request");
	stream<ap_uint<SLOT_ID_WIDTH>> check_full("dummy check full");
	stream<bool> tx_fin("dummy tx completion sig");
	stream<ap_uint<SLOT_ID_WIDTH> > rt_timeout("dummy timeout sig");
	stream<struct conn_mgmt_req> init_req("initiate request");

	test_query.src_ip = 0xc0a80102;  // 192.168.1.2
	test_query.dest_ip = 0xc0a80180; // 192.168.1.128

	init.set_type = GBN_SOC2FPGA_SET_TYPE_OPEN;
	init.slotid = 10;
	init_req.write(init);
	test_rx_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
				   &rt_timeout, &init_req);

	for (int i = 0; cycle < MAX_CYCLE; i++) {
		if (i < test_seq.size()) {
			ses_id = build_sesid(20, 10);
			test_query.gbn_header =
				build_gbn_header(ses_id, GBN_PKT_DATA,
						 test_seq[i], 0).data;
			dph("[cycle %2d] host send gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
			    cycle, test_query.gbn_header(7, 0).to_uint(),
			    test_query.gbn_header(7 + SEQ_WIDTH, 8).to_uint64(),
			    ses_id(SLOT_ID_WIDTH - 1, 0).to_uint(),
			    ses_id(2 * SLOT_ID_WIDTH - 1, SLOT_ID_WIDTH).to_uint());

			rx_query.write(test_query);
		}
		test_rx_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
					   &rt_timeout, &init_req);
	}

	printf("-------test rx done---------\n");
}

void test_ack(vector<unsigned> &ack_seq, vector<enum gbn_pkt_type> &ack_type)
{
	printf("----------test ack-----------\n");

	test_util test_ack_util;
	vector<unsigned> send_seq = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	struct query_req test_query;
	struct conn_mgmt_req init;

	ap_uint<SLOT_ID_WIDTH> slot_id = 10;
	ap_uint<SES_ID_WIDTH> ses_id;
	stream<struct query_req> rx_query("receive query request");
	stream<ap_uint<SLOT_ID_WIDTH>> check_full("dummy check full request");
	stream<bool> tx_fin("dummy tx completion sig");
	stream<ap_uint<SLOT_ID_WIDTH> > rt_timeout("dummy timeout sig");
	stream<struct conn_mgmt_req> init_req("initiate request");

	test_query.src_ip = 0xc0a80102;  // 192.168.1.2
	test_query.dest_ip = 0xc0a80180; // 192.168.1.128

	init.set_type = GBN_SOC2FPGA_SET_TYPE_OPEN;
	init.slotid = slot_id;
	init_req.write(init);
	test_ack_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
				    &rt_timeout, &init_req);

	for (int i = 0; cycle < MAX_CYCLE; i++) {
		if (i < send_seq.size()) {
			check_full.write(slot_id);
			tx_fin.write(true);
		}
		test_ack_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
					   &rt_timeout, &init_req);
	}

	for (int i = 0; cycle < 2 * MAX_CYCLE; i++) {
		if (i < ack_seq.size()) {
			ses_id = build_sesid(20, 10);
			test_query.gbn_header = build_gbn_header(ses_id, ack_type[i],
						      ack_seq[i], 0).data;
			dph("[cycle %2d] host send gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
			    cycle, test_query.gbn_header(7, 0).to_uint(),
			    test_query.gbn_header(7 + SEQ_WIDTH, 8).to_uint64(),
			    ses_id(SLOT_ID_WIDTH - 1, 0).to_uint(),
			    ses_id(2 * SLOT_ID_WIDTH - 1, SLOT_ID_WIDTH).to_uint());

			rx_query.write(test_query);
		}
		test_ack_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
					   &rt_timeout, &init_req);
	}

	printf("-------test ack done---------\n");
}

void test_consistency()
{
	test_util test_cons_util;
	struct query_req test_query;
	struct conn_mgmt_req init;

	ap_uint<SLOT_ID_WIDTH> slot_id = 10;
	ap_uint<SES_ID_WIDTH> ses_id;
	stream<struct query_req> rx_query("receive query request");
	stream<ap_uint<SLOT_ID_WIDTH>> check_full("check full request");
	stream<bool> tx_fin("dummy tx completion sig");
	stream<ap_uint<SLOT_ID_WIDTH> > rt_timeout("dummy timeout sig");
	stream<struct conn_mgmt_req> init_req("initiate request");

	test_query.src_ip = 0xc0a80102;  // 192.168.1.2
	test_query.dest_ip = 0xc0a80180; // 192.168.1.128

	init.set_type = GBN_SOC2FPGA_SET_TYPE_OPEN;
	init.slotid = slot_id;
	init_req.write(init);
	test_cons_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
				     &rt_timeout, &init_req);
	
	ses_id = build_sesid(20, 10);
	test_query.gbn_header = build_gbn_header(ses_id, GBN_PKT_DATA, 2, 0).data;
	rx_query.write(test_query);
	test_query.gbn_header = build_gbn_header(ses_id, GBN_PKT_DATA, 2, 0).data;
	rx_query.write(test_query);

	for (; cycle < MAX_CYCLE; )
		test_cons_util.run_one_cycle(&rx_query, &check_full, &tx_fin,
					     &rt_timeout, &init_req);
}

int main()
{
	vector<unsigned> seq1 = {1, 2, 3, 4, 5, 6, 7};
	vector<unsigned> seq2 = {1, 2, 3, 5, 6, 7};
	vector<unsigned> seq3 = {1, 2, 3, 5, 6, 7, 4, 8};
	vector<unsigned> seq4 = {1, 2, 3, 5, 6, 2, 3, 4, 5, 6, 2};
	// test_rx(seq1);
	// test_rx(seq2);
	// test_rx(seq3);
	// test_rx(seq4);

	vector<unsigned> seqack1 = {1, 2, 3, 2, 5, 6, 7};
	vector<enum gbn_pkt_type> type1(7, GBN_PKT_ACK);
	vector<unsigned> seqack2 = {1, 2, 3, 3, 6, 8};
	vector<enum gbn_pkt_type> type2 = {GBN_PKT_ACK, GBN_PKT_ACK,
				       GBN_PKT_ACK, GBN_PKT_NACK,
				       GBN_PKT_ACK, GBN_PKT_ACK};
	test_ack(seqack1, type1);
	test_ack(seqack2, type2);

	// test_consistency();
}
