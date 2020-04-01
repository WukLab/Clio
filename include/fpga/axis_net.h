/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

/*
 * This file descirbes network interfaces used by FPGA IPs.
 * This file is supposed to be used by FPGA code only.
 */

#ifndef _LEGO_MEM_AXIS_NET_
#define _LEGO_MEM_AXIS_NET_

#include <ap_axi_sdata.h>
#include <hls_stream.h>

#define NR_BYTES_AXIS_64	(8)
#define NR_BYTES_AXIS_256	(32)
#define NR_BYTES_AXIS_512	(64)

struct net_axis_64 {
	ap_uint<64>			data;
	ap_uint<1>			last;
	ap_uint<NR_BYTES_AXIS_64>	keep;
	ap_uint<1>			user;
};

/*
 * For 256b version, we will have this header format:
 * | Eth Header | App Header |
 * 0            112
 * There will not be other headers.
 */
struct net_axis_256 {
	ap_uint<256>			data;
	ap_uint<1>			last;
	ap_uint<NR_BYTES_AXIS_256>	keep;
	ap_uint<1>			user;
};

struct net_axis_512 {
	ap_uint<512>			data;
	ap_uint<1>			last;
	ap_uint<NR_BYTES_AXIS_512>	keep;
	ap_uint<1>			user;
};

struct udp_info {
	ap_uint<32>			src_ip;
	ap_uint<32>			dest_ip;
	ap_uint<16>			src_port;
	ap_uint<16>			dest_port;
	ap_uint<16>			length;
};

#endif /* _LEGO_MEM_AXIS_NET_ */
