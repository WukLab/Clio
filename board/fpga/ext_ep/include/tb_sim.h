/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_TB_SIM_H_
#define _LEGO_MEM_TB_SIM_H_

#include <map>
#include <vector>
#include <ap_int.h>
#include <hls_stream.h>
#include <pattern_generator.h>
#include <ext_ep_types.h>
#include <ext_ep.h>

struct ctrl {
	struct ctrl_if		pkt;
	bool			vld;
} __packed;

struct data {
	struct data_if		pkt;
	bool 			vld;
};

// core_mem simulation
enum mem_fsm {MEM_HEADER, MEM_WRITEDATA, MEM_READDATA};
class MemSim {
public:
	std::vector<uint8_t> memory;

	MemSim(unsigned long size, unsigned int latency);
	~MemSim() {}

	/* core_mem simulator run one cycle */
	void mem_sim(hls::stream<struct data_if> &data_in,
		     hls::stream<struct data_if> &data_out);

private:
	const unsigned long mem_size;
	const unsigned int latency;
	hls::stream<struct data_if> parse2delay;
	std::vector<struct data> delay_fifo;
	mem_fsm state;
	unsigned long rw_addr, rw_size;
	struct legomem_rw_fpgamsg_resp write_resp_wait;

	void parse(hls::stream<struct data_if> &data_in,
		   hls::stream<struct data_if> &to_delay);
	void delay(hls::stream<struct data_if> &from_parse,
		   hls::stream<struct data_if> &data_out);
	void rwmem(bool rw, unsigned long addr,
		   unsigned long size, char *data);
};

// soc simulation
struct sim_used_proc_create {
	struct lego_header		hdr;
} __packed;
struct sim_used_proc_create_ret {
	struct lego_header		hdr;
	struct op_create_proc_resp	op;
} __packed;

class SocSim {
public:
	SocSim(unsigned long mem_size,
	       unsigned int data_latency, unsigned int ctrl_latency);
	~SocSim() {}

	void soc_sim(hls::stream<struct ctrl_if> &ctrl_in,
		     hls::stream<struct ctrl_if> &ctrl_out,
		     hls::stream<struct data_if> &data_in,
		     hls::stream<struct data_if> &data_out);

private:
	const unsigned long mem_size;
	const unsigned int data_latency;
	const unsigned int ctrl_latency;
	unsigned int pid;
	unsigned long alloc_map;
	std::vector<struct data> data_delay_fifo;
	std::vector<struct ctrl> ctrl_delay_fifo;
	hls::stream<struct data_if> parse2datadelay;
	hls::stream<struct ctrl_if> parse2ctrldelay;

	void parse(hls::stream<struct ctrl_if> &ctrl_in,
		   hls::stream<struct ctrl_if> &ctrl_to_delay,
		   hls::stream<struct data_if> &data_in,
		   hls::stream<struct data_if> &data_to_delay);
	void ctrl_delay(hls::stream<struct ctrl_if> &from_parse,
			hls::stream<struct ctrl_if> &ctrl_out);
	void data_delay(hls::stream<struct data_if> &from_parse,
			hls::stream<struct data_if> &data_out);
};

// net simulation
enum net_fsm {NET_SEND, NET_RECV, NET_DONE};
enum read_receving_fsm {NET_RW_HEADER, NET_RW_DATA};
class NetSim {
public:
	Patterns objid_pattern;
	net_fsm state;
	read_receving_fsm read_state, receiver_state;

	NetSim(int pattern_len, int num_client);
	~NetSim() {}

	void state_reset();
	bool net_sim_proc_create(hls::stream<struct data_if> &data_in,
				 hls::stream<struct data_if> &data_out);
	bool net_sim_obj_create(hls::stream<struct data_if> &data_in,
				hls::stream<struct data_if> &data_out);
	bool net_sim_write_once(hls::stream<struct data_if> &data_in,
				hls::stream<struct data_if> &data_out);
	bool net_sim(hls::stream<struct data_if> &data_in,
		     hls::stream<struct data_if> &data_out);

private:
	const unsigned long pattern_len;
	const unsigned long num_client;
	std::vector<unsigned long> client_pids;
	std::vector<uint32_t> objids;
	std::map<uint32_t, uint16_t> objid_sizes;
	std::map<uint32_t, uint16_t> objid_pids;
	std::map<uint32_t, uint16_t> objid_data;
	std::queue<uint32_t> objid_recorded;
	std::queue<std::vector<char>> expected_outcome;
	std::queue<uint16_t> obj_create_sizes;
	std::queue<uint16_t> obj_create_pids;
	int received, total_sent, bytes_read;
	uint32_t receiving_id;
	char obj_specific_data;
	char write_data_buffer[2*DATASIZE];
	char read_data_buffer[3*DATASIZE];

	void proc_create(hls::stream<struct data_if> &out);
	long proc_create_getreply(hls::stream<struct data_if> &in);
	void verobj_create(uint16_t pid, unsigned long size, unsigned long flags,
			hls::stream<struct data_if> &out);
	long verobj_create_getreply(hls::stream<struct data_if> &in);
	void verobj_delete(uint16_t pid, unsigned long obj_id,
			hls::stream<struct data_if> &out);
	void verobj_read(uint16_t pid, unsigned long obj_id, unsigned short version,
			hls::stream<struct data_if> &out);
	void verobj_write(uint16_t pid, unsigned long obj_id, int size,
			hls::stream<struct data_if> &out);
	int verobj_rw_getreply(hls::stream<struct data_if> &in);

	// helper function
	uint16_t new_obj_size();
	void prep_write_buffer(char data, int size, uint16_t identifier);
	void print_data(std::vector<char> data);
	bool compare_data(std::vector<char> first, std::vector<char> second);
};

// Xbar simulation
enum xbar_fsm {XBAR_HEADER, XBAR_NET_DATA, XBAR_MEM_DATA, XBAR_EXTEP_DATA};
class XbarSim {
public:
	XbarSim();
	~XbarSim() {}

	void xbar_sim(hls::stream<struct ctrl_if> &to_extep_ctrl,
		      hls::stream<struct ctrl_if> &from_extep_ctrl,
		      hls::stream<struct ctrl_if> &to_soc_ctrl,
		      hls::stream<struct ctrl_if> &from_soc_ctrl,
		      hls::stream<struct data_if> &to_net_data,
		      hls::stream<struct data_if> &from_net_data,
		      hls::stream<struct data_if> &to_extep_data,
		      hls::stream<struct data_if> &from_extep_data,
		      hls::stream<struct data_if> &to_soc_data,
		      hls::stream<struct data_if> &from_soc_data,
		      hls::stream<struct data_if> &to_mem_data,
		      hls::stream<struct data_if> &from_mem_data);

private:
	xbar_fsm state;
	uint16_t dest;
};

#endif /* _LEGO_MEM_TB_SIM_H_ */
