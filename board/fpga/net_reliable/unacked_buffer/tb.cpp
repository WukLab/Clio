#include <stdio.h>
#include <vector>
#include "unacked_buffer.hpp"
#define MAX_CYCLE 100

using namespace std;

int cycle = 0;

class test_util {
    public:
	test_util();
	~test_util() {}
	void run_one_cycle(stream<struct net_axis_64> *tx_buff_payload,
			   stream<struct route_info> *tx_buff_route_info,
			   stream<struct retrans_req> *gbn_retrans_req);

    private:
    	stream<struct timer_req> timer_rst_req;
	stream<struct net_axis_64> rt_payload;
	stream<struct udp_info> rt_header;
	stream<struct dm_cmd> dm_rd_cmd_1;
	stream<struct dm_cmd> dm_rd_cmd_2;
	stream<struct net_axis_64> dm_rd_data;
	stream<struct dm_cmd> dm_wr_cmd;
	stream<struct net_axis_64> dm_wr_data;
};

test_util::test_util()
	: timer_rst_req("timer reset"),
	  rt_payload("retrans payload"), rt_header("retrans udp header"),
	  dm_rd_cmd_1("datamover read cmd"), dm_rd_cmd_2("datamover read cmd"),
	  dm_rd_data("datamover read data"), dm_wr_cmd("data mover write cmd"),
	  dm_wr_data("datamover write data")
{
	cycle = 0;
}

void test_util::run_one_cycle(stream<struct net_axis_64> *tx_buff_payload,
			      stream<struct route_info> *tx_buff_route_info,
			      stream<struct retrans_req> *gbn_retrans_req)
{
	struct udp_info recv_hd;
	struct net_axis_64 recv_data;

	unacked_buffer(&timer_rst_req, tx_buff_payload,
		       tx_buff_route_info, gbn_retrans_req,
		       &rt_payload, &rt_header,
		       &dm_rd_cmd_1, &dm_rd_cmd_2, &dm_rd_data,
		       &dm_wr_cmd, &dm_wr_data);
	
	if (!timer_rst_req.empty()) {
		struct timer_req rt_timer = timer_rst_req.read();
		printf("[cycle %2d] send timer reset [type %d, slot %d]\n", cycle,
		    rt_timer.rst_type.to_uint(), rt_timer.slotid.to_uint());
	}

	if (!rt_payload.empty()) {
		recv_data = rt_payload.read();
		printf("[cycle %2d] send data to net %llx, ", cycle,
		    recv_data.data.to_uint64());
		ap_uint<8> type = recv_data.data(PKT_TYPE_OFFSET + PKT_TYPE_WIDTH - 1, PKT_TYPE_OFFSET);
		ap_uint<SEQ_WIDTH> seqnum =
		    recv_data.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		ap_uint<SLOT_ID_WIDTH> src_slot = recv_data.data(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		ap_uint<SES_ID_WIDTH> dest_slot = recv_data.data(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		printf("if gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
		    type.to_ushort(), seqnum.to_uint64(), src_slot.to_uint(),
		    dest_slot.to_uint());
	}

	if (!rt_header.empty()) {
		recv_hd = rt_header.read();
		printf("[cycle %2d] send data to net %x:%d -> %x:%d\n", cycle,
		    recv_hd.src_ip.to_uint(), recv_hd.src_port.to_uint(),
		    recv_hd.dest_ip.to_uint(), recv_hd.dest_port.to_uint());
	}

	if (!dm_rd_cmd_1.empty()) {
		struct dm_cmd cmd = dm_rd_cmd_1.read();
		printf("[cycle %2d] datamover receive read cmd [from %lx, length %d]\n",
		    cycle, cmd.start_address.to_uint(), cmd.btt.to_uint());
		struct net_axis_64 length;
		length.data = 0x18;
		length.keep = 0xff;
		length.last = 1;
		dm_rd_data.write(length);
	}

	if (!dm_rd_cmd_2.empty()) {
		struct dm_cmd cmd = dm_rd_cmd_2.read();
		printf("[cycle %2d] datamover receive read cmd [from %lx, length %d]\n",
		    cycle, cmd.start_address.to_uint(), cmd.btt.to_uint());
		struct net_axis_64 rd_data;
		for (int j = 0; j < 2; j++) {
			rd_data.data = 0x0f0f0f0f0f0f0f0f;
			rd_data.last = 0;
			dm_rd_data.write(rd_data);
		}
		rd_data.data = 42;
		rd_data.last = 1;
		dm_rd_data.write(rd_data);
	}

	if (!dm_wr_cmd.empty()) {
		struct dm_cmd cmd = dm_wr_cmd.read();
		printf("[cycle %2d] datamover receive write cmd [from %lx, length %d]\n",
		    cycle, cmd.start_address.to_uint(), cmd.btt.to_uint());
	}

	if (!dm_wr_data.empty()) {
		struct net_axis_64 wr_data = dm_wr_data.read();
		printf("[cycle %2d] datamover receive write data %llx, ", cycle,
		    wr_data.data.to_uint64());
		ap_uint<8> type = wr_data.data(PKT_TYPE_OFFSET + PKT_TYPE_WIDTH - 1, PKT_TYPE_OFFSET);
		ap_uint<SEQ_WIDTH> seqnum =
		    wr_data.data(SEQ_OFFSET + SEQ_WIDTH - 1, SEQ_OFFSET);
		ap_uint<SLOT_ID_WIDTH> src_slot = wr_data.data(
			SRC_SLOT_OFFSET + SLOT_ID_WIDTH - 1, SRC_SLOT_OFFSET);
		ap_uint<SES_ID_WIDTH> dest_slot = wr_data.data(
			DEST_SLOT_OFFSET + SLOT_ID_WIDTH - 1, DEST_SLOT_OFFSET);
		printf("if gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
		    type.to_ushort(), seqnum.to_uint64(), src_slot.to_uint(),
		    dest_slot.to_uint());
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

void test_write()
{
	printf("----------test write-----------\n");

	test_util buff_util;
	vector<unsigned> test_seq = { 1000201, 1000202, 1000203 };

	struct route_info test_header;
	struct net_axis_64 test_payload;
	ap_uint<SES_ID_WIDTH> ses_id;

	
	stream<struct net_axis_64> buff_payload("buffer payload");
	stream<struct route_info> buff_route_info("buffer ip route");
	stream<struct retrans_req> gbn_retrans_req("dummy retrans");

	test_header.dest_ip = 0xc0a80180;
	test_header.length = 4 * 8;

	test_payload.keep = 0xff;
	test_payload.user = 0;

	for (int i = 0; cycle < MAX_CYCLE; i++) {
		if (cycle < test_seq.size()) {
			dph("[cycle %2d] host send -> %x, length %d\n", cycle,
			    test_header.dest_ip.to_uint(),
			    test_header.length.to_uint());
			buff_route_info.write(test_header);

			ses_id = build_sesid(20, 10);
			test_payload = build_gbn_header(ses_id, GBN_PKT_DATA,
							test_seq[i], 0);
			dph("[cycle %2d] host send gbn header [type %d, seq %lld, src slot %d, dest slot %d]\n",
			    cycle, test_payload.data(7, 0).to_uint(),
			    test_payload.data(7 + SEQ_WIDTH, 8).to_uint64(),
			    ses_id(SLOT_ID_WIDTH - 1, 0).to_uint(),
			    ses_id(2 * SLOT_ID_WIDTH - 1, SLOT_ID_WIDTH).to_uint());

			buff_payload.write(test_payload);
			for (int j = 0; j < 2; j++) {
				test_payload.data = 0x0f0f0f0f0f0f0f0f;
				test_payload.last = 0;
				buff_payload.write(test_payload);
			}
			test_payload.data = test_seq[i];
			test_payload.last = 1;
			buff_payload.write(test_payload);
		}
		buff_util.run_one_cycle(&buff_payload, &buff_route_info,
					&gbn_retrans_req);
	}

	printf("-------test write done---------\n");

	printf("----------test read-----------\n");

	struct retrans_req rt_req;
	rt_req.seq_start = 200;
	rt_req.seq_end = 205;
	rt_req.slotid = 20;

	gbn_retrans_req.write(rt_req);

	for (; cycle < 2 * MAX_CYCLE; ) {
		buff_util.run_one_cycle(&buff_payload, &buff_route_info,
					&gbn_retrans_req);
	}

	printf("-------test read done---------\n");
}

int main()
{
	test_write();
}
