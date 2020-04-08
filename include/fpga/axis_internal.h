#ifndef _LEGO_MEM_AXIS_INTERNAL_
#define _LEGO_MEM_AXIS_INTERNAL_

#include <uapi/gbn.h>
#include <ap_int.h>

enum timer_rst_type {
	timer_rst_type_reset = 1,
	timer_rst_type_stop
};

struct query_req {
	ap_uint<64>		gbn_header;
	ap_uint<32>		src_ip;
	ap_uint<32>		dest_ip;
};

struct route_ip {
	ap_uint<32>		src_ip;
	ap_uint<32>		dest_ip;
};

struct route_info {
	struct route_ip		ip_info;
	ap_uint<16>		length;
};

struct retrans_req {
	ap_uint<SLOT_ID_WIDTH>	slotid;
	ap_uint<SEQ_WIDTH>	seq_start;
	ap_uint<SEQ_WIDTH>	seq_end;
};

struct timer_req {
	ap_uint<SLOT_ID_WIDTH>		slotid;
	ap_uint<16 - SLOT_ID_WIDTH>	rst_type;
};

struct conn_mgmt_req {
	ap_uint<SLOT_ID_WIDTH>		slotid;
	ap_uint<16 - SLOT_ID_WIDTH>	set_type;
};

#endif
