/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <tb_sim.h>
#include <iostream>
#include <stdexcept>

//#define NET_SIM_PRINT

using namespace hls;
using namespace std;

NetSim::NetSim(int pattern_len, int num_client)
	: objid_pattern(pattern_len),
	  pattern_len(pattern_len),
	  num_client(num_client)
{
	state = NET_SEND;
	read_state = NET_RW_HEADER;
	receiver_state = NET_RW_HEADER;
	received = 0;
	total_sent = 0;
	bytes_read = 0;
	receiving_id = 0;
	obj_specific_data = 0xAA;
	cout << "Simulated Network Initialized" << endl;
}

void NetSim::state_reset()
{
	state = NET_SEND;
	read_state = NET_RW_HEADER;
	receiver_state = NET_RW_HEADER;
	received = 0;
	total_sent = 0;
	bytes_read = 0;
	receiving_id = 0;
}

bool NetSim::net_sim_proc_create(stream<struct data_if> &to_xbar, stream<struct data_if> &from_xbar)
{
	long pid = 0;

	switch (state) {
	case NET_SEND:
		for (int i = 0; i < num_client; i++)
			proc_create(to_xbar);
		state = NET_RECV;
		break;

	case NET_RECV:
		pid = proc_create_getreply(from_xbar);
		if (pid > 0) {
			cout << "new PID: " << pid << endl;
			client_pids.push_back(pid);
		}
		if (client_pids.size() == num_client) {
			state = NET_DONE;
			return true;
		}
		break;

	case NET_DONE:
		return true;
	}
	return false;
}

bool NetSim::net_sim_obj_create(stream<struct data_if> &to_xbar, stream<struct data_if> &from_xbar)
{
	long obj_id = 0;
	switch (state) {
	case NET_SEND:
		for (int i = 0; i < pattern_len; i++) {
			uint16_t size = new_obj_size();
			uint16_t pid = client_pids[i % client_pids.size()];
			obj_create_sizes.push(size);
			obj_create_pids.push(pid);
			verobj_create(pid, size, 0, to_xbar);
		}
		state = NET_RECV;
		break;

	case NET_RECV:
		obj_id = verobj_create_getreply(from_xbar);
		if (obj_id > 0) {
			uint16_t size = obj_create_sizes.front();
			uint16_t pid = obj_create_pids.front();
			obj_create_sizes.pop();
			obj_create_pids.pop();
			objids.push_back(obj_id);
			objid_sizes.insert(pair<uint32_t, uint16_t>(obj_id, size));
			objid_pids.insert(pair<uint32_t, uint16_t>(obj_id, pid));
			objid_data.insert(pair<uint32_t, uint16_t>(obj_id, obj_specific_data));
			obj_specific_data++;
			printf("# %d, obj_id: %4d, size: %3d, pid: %3d, data: 0x%02X\n",
				objid_sizes.size(), obj_id, size,
				pid, obj_specific_data & 0xFF);
		}
		if (objid_sizes.size() == pattern_len) {
			state = NET_DONE;
			return true;
		}
		break;

	case NET_DONE:
		return true;
	default:
		break;
	}
	return false;
}

bool NetSim::net_sim_write_once(stream<struct data_if> &to_xbar, stream<struct data_if> &from_xbar)
{
	int reply;
	const uint16_t identifier = 1;

	switch (state) {
	case NET_SEND:
		for (std::vector<uint32_t>::iterator it = objids.begin();
			it != objids.end(); it++) {
			uint16_t size = objid_sizes[(*it)];
			uint16_t pid = objid_pids[(*it)];
			prep_write_buffer(objid_data[(*it)], objid_sizes[(*it)], identifier);
			verobj_write(pid, (*it), size, to_xbar);
		}
		state = NET_RECV;
		break;

	case NET_RECV:
		assert(received <= objid_sizes.size());
		reply = verobj_rw_getreply(from_xbar);
		if (reply == 0) {
			received++;
			cout << "# " << received << " received" << endl;
		}
		if (objid_sizes.size() == received) {
			state = NET_DONE;
			return true;
		}
		break;

	case NET_DONE:
		return true;
	}
	return false;
}

bool NetSim::net_sim(stream<struct data_if> &to_xbar,
		     stream<struct data_if> &from_xbar)
{
	int sequence_cnt = 0, reply = 0;
	map<uint32_t, uint16_t> objid_identifier;
	list<vector<int> > pattern;

	switch (state) {
	case NET_SEND:
		pattern = objid_pattern.unlabeled_obj_to_boxes();
		for (list<vector<int> >::iterator it = pattern.begin(); it != pattern.end(); it++) {
			for (ap_uint<64> rw_seq = 0; rw_seq < (1 << pattern_len); rw_seq++) {
				sequence_cnt++;
				for (int idx = 0; idx < pattern_len; idx++) {
					total_sent++;
					uint32_t id = objids[(*it)[idx]];
#ifdef NET_SIM_PRINT
					cout << "sequence: " << sequence_cnt << ",  total: " << total_sent
						<< ",  rw: " << rw_seq[idx] << ",  id: " << id
						<< ",  size: " << objid_sizes[id] << endl;
#endif
					if (objid_identifier.count(id) == 0)
						objid_identifier.insert(pair<uint32_t, uint16_t>(id, 1));
					if (rw_seq[idx] == 1) {
						/* if write request, don't record write response,
						 * identify correctness by checking subsequent read */
						objid_identifier[id]++;
						prep_write_buffer(objid_data[id], objid_sizes[id], objid_identifier[id]);
						verobj_write(objid_pids[id], id, objid_sizes[id], to_xbar);
					} else {
						/* if read request */
						prep_write_buffer(objid_data[id], objid_sizes[id], objid_identifier[id]);
						vector<char> expected(write_data_buffer,
								write_data_buffer + objid_sizes[id]);
						expected_outcome.push(expected);
						objid_recorded.push(id);
						verobj_read(objid_pids[id], id, 0, to_xbar);
					}
				}
			}
		}
		cout << "total sequence: " << sequence_cnt
		     << " total request sent: " << total_sent << endl;
		state = NET_RECV;
		break;

	case NET_RECV:
		reply = verobj_rw_getreply(from_xbar);
		if (reply < 0)
			break;

		/* receive a write reply */
		if (reply == 0) {
			received++;
			goto receive_done;
		}

		/* read reply */
		bytes_read += reply;
		switch (receiver_state) {
		case NET_RW_HEADER:
                        receiving_id = objid_recorded.front();
                        objid_recorded.pop();
                        if (bytes_read < objid_sizes[receiving_id])
				receiver_state = NET_RW_DATA;
                        break;
		case NET_RW_DATA:
			if (bytes_read == objid_sizes[receiving_id])
				receiver_state = NET_RW_HEADER;
			break;
                }
		assert(bytes_read <= objid_sizes[receiving_id]);
		if (bytes_read == objid_sizes[receiving_id]) {
			vector<char> expected = expected_outcome.front();
			vector<char> pkt_read(read_data_buffer,
					read_data_buffer + objid_sizes[receiving_id]);
			bool same_data = compare_data(expected, pkt_read);
			expected_outcome.pop();

			if (!same_data) {
				cout << "received: " << ++received << endl;
				cout << "expected: ";
				print_data(expected);
				cout << "real:     ";
				print_data(pkt_read);
				fflush(stdout);
				throw logic_error("Data Read not correct");
			}
			received++;
			bytes_read = 0;
			receiving_id = 0;
		}

receive_done:
		if (received == total_sent) {
			state = NET_DONE;
			return true;
		}
		break;

	case NET_DONE:
		return true;
	default:
		break;
	}
	return false;
}

void NetSim::proc_create(stream<struct data_if> &out)
{
	struct data_if data 		= {0};
	struct sim_used_proc_create req = {0};

	req.hdr.opcode = OP_CREATE_PROC;
	req.hdr.size = sizeof(struct lego_header);
	req.hdr.cont = LEGOMEM_CONT_SOC;
	memcpy(&data.pkt, &req, sizeof(struct lego_header));
	data.last = 1;
	out.write(data);
}

long NetSim::proc_create_getreply(stream<struct data_if> &in)
{
	struct data_if data 		= {0};
	struct sim_used_proc_create_ret *resp;

	if (in.empty())
		return -1;

	data = in.read();
	assert(data.last == 1);
	resp = (struct sim_used_proc_create_ret *)&data.pkt;
	if (resp->hdr.opcode != OP_CREATE_PROC_RESP)
		throw logic_error("CREATE_PROC: simulator error, wrong opcode");
	if (resp->hdr.size != sizeof(struct sim_used_proc_create_ret))
		throw logic_error("CREATE_PROC: simulator error, wrong size");
	if (resp->hdr.req_status != 0 || resp->op.ret != 0)
		throw logic_error("CREATE_PROC: proc create bad reply status");

	return resp->op.pid;
}

void NetSim::verobj_create(uint16_t pid, unsigned long size, unsigned long flags,
			  stream<struct data_if> &out)
{
	struct data_if data 		= {0};
	struct verobj_create_delete req	= {0};

	req.hdr.pid = pid;
	req.hdr.opcode = OP_REQ_VEROBJ_CREATE;
	req.hdr.size = sizeof(struct verobj_create_delete);
	req.hdr.cont = LEGOMEM_CONT_EXTAPI;
	req.op.obj_size_id = size;
	req.op.vregion_idx = 0;
	req.op.vm_flags = flags;

	memcpy(&data.pkt, &req, sizeof(struct verobj_create_delete));
	data.last = 1;
	out.write(data);
}

long NetSim::verobj_create_getreply(stream<struct data_if> &in)
{
	struct data_if data 		= {0};
	struct verobj_create_delete_ret *resp;

	if (in.empty())
		return -1;

	data = in.read();
	assert(data.last == 1);
	resp = (struct verobj_create_delete_ret *)&data.pkt;
	if (resp->hdr.opcode != OP_REQ_VEROBJ_CREATE_RESP)
		throw logic_error("CREATE: implementation error, wrong opcode");
	if (resp->hdr.size != sizeof(struct verobj_create_delete_ret))
		throw logic_error("CREATE: implementation error, wrong size");
	if (resp->hdr.req_status != 0)
		throw logic_error("CREATE: object create bad reply status");

	return resp->op.obj_id;
}

void NetSim::verobj_delete(uint16_t pid, unsigned long obj_id,
			  stream<struct data_if> &out)
{
	struct data_if data		= {0};
	struct verobj_create_delete req	= {0};

	req.hdr.pid = pid;
	req.hdr.opcode = OP_REQ_VEROBJ_DELETE;
	req.hdr.size = sizeof(struct verobj_create_delete);
	req.hdr.cont = LEGOMEM_CONT_EXTAPI;
	req.op.obj_size_id = obj_id;
	req.op.vregion_idx = 0;
	req.op.vm_flags = 0;

	memcpy(&data.pkt, &req, sizeof(struct verobj_create_delete));
	data.last = 1;
	out.write(data);
}

void NetSim::verobj_read(uint16_t pid, unsigned long obj_id, unsigned short version,
			stream<struct data_if> &out)
{
	struct data_if data 		= {0};
	struct verobj_read_write req 	= {0};

	req.hdr.pid = pid;
	req.hdr.opcode = OP_REQ_VEROBJ_READ;
	req.hdr.size = sizeof(struct verobj_read_write);
	req.hdr.cont = LEGOMEM_CONT_EXTAPI;
	req.op.obj_id = obj_id;
	req.op.version = version;

	memcpy(&data.pkt, &req, sizeof(struct verobj_read_write));
	data.last = 1;
	out.write(data);
}

int NetSim::verobj_rw_getreply(stream<struct data_if> &in)
{
	static int data_remain			= 0;
	static int offset			= 0;
	struct data_if data			= {0};
	struct verobj_read_write_ret *resp;
	int size 				= 0;

	if (in.empty())
		return -1;

	data = in.read();
	switch (read_state) {
	case NET_RW_HEADER:
		resp = (struct verobj_read_write_ret *)&data.pkt;
		if (resp->hdr.opcode != OP_REQ_VEROBJ_WRITE_RESP &&
				resp->hdr.opcode != OP_REQ_VEROBJ_READ_RESP)
			throw logic_error("RW: implementation error, wrong opcode");
		if (resp->hdr.opcode == OP_REQ_VEROBJ_READ_RESP) {
			if (resp->hdr.size == sizeof(struct verobj_read_write_ret))
				throw logic_error("READ: implementation error, zero size");
			if (resp->hdr.size > 3*DATASIZE)
				throw logic_error("READ: size too large, read buffer will overflow");
		} else {
			if (resp->hdr.size != sizeof(struct verobj_read_write_ret))
				throw logic_error("WRITE: implementation error, size is wrong");
		}
		if (resp->hdr.req_status != 0)
			throw logic_error("RW: object read bad reply status");

		if (resp->hdr.opcode == OP_REQ_VEROBJ_WRITE_RESP) {
			assert(data.last == 1);
			return 0;
		}

		/* read reply */
		if (resp->hdr.size > DATASIZE) {
			assert(data.last == 0);
			size = DATASIZE - sizeof(struct verobj_read_write_ret);
			read_state = NET_RW_DATA;
			offset = size;
			data_remain = resp->hdr.size - DATASIZE;
		} else {
			assert(data.last == 1);
			offset = 0;
			size = resp->hdr.size - sizeof(struct verobj_read_write_ret);
		}
		memcpy(read_data_buffer, resp->op.data, size);
		break;

	case NET_RW_DATA:
		if (data_remain > DATASIZE) {
			assert(data.last == 0);
			size = DATASIZE;
			data_remain -= DATASIZE;
		} else {
			assert(data.last == 1);
			size = data_remain;
			data_remain = 0;
			read_state = NET_RW_HEADER;
		}
		assert(offset + size <= 2*DATASIZE);
		memcpy(&read_data_buffer[offset], &data.pkt, size);
		offset += size;
		break;
	}
	return size;
}

void NetSim::verobj_write(uint16_t pid, unsigned long obj_id, int size, stream<struct data_if> &out)
{
	struct data_if data 		= {0};
	char local_buffer[DATASIZE]	= {0};
	int offset 			= 0;
	struct verobj_read_write req 	= {0};

	req.hdr.pid = pid;
	req.hdr.opcode = OP_REQ_VEROBJ_WRITE;
	req.hdr.size = sizeof(struct verobj_read_write) + size;
	req.hdr.cont = LEGOMEM_CONT_EXTAPI;
	req.op.obj_id = obj_id;
	req.op.version = 0;

	memcpy(&local_buffer, &req, sizeof(struct verobj_read_write));
	if (size > DATASIZE - sizeof(struct verobj_read_write)) {
		memcpy(&local_buffer[sizeof(struct verobj_read_write)],
			write_data_buffer, DATASIZE - sizeof(struct verobj_read_write));
		size -= (DATASIZE - sizeof(struct verobj_read_write));
		offset += (DATASIZE - sizeof(struct verobj_read_write));
		data.last = 0;
	} else {
		memcpy(&local_buffer[sizeof(struct verobj_read_write)],
				write_data_buffer, size);
		size = 0;
		data.last = 1;
	}
	memcpy(&data.pkt, &local_buffer, DATASIZE);
	out.write(data);

	while (size > 0) {
		data = {0};
		if (size > DATASIZE) {
			memcpy(&data.pkt, &write_data_buffer[offset], DATASIZE);
			data.last = 0;
			size -= DATASIZE;
			offset += DATASIZE;
		} else {
			memcpy(&data.pkt, &write_data_buffer[offset], size);
			data.last = 1;
			size = 0;
		}
		out.write(data);
	}
}

uint16_t NetSim::new_obj_size()
{
	/*
	 * for testing purpose, use 5 different obj size
	 * 1. obj size + parameter < DATASIZE
	 *    fsm don't need to go to extra DATA state
	 * 2. obj size + parameter == DATASIZE
	 *    similar to 1, edge case
	 * 3. 2*DATASIZE > obj size + parameter > DATASIZE
	 *    fsm don't need transition from HEADER to DATA state
	 * 4. obj size + parameter == 2*DATASIZE
	 *    similar to 3, edge case
	 * 5. obj size + parameter > 2*DATASIZE
	 *    fsm stays at DATA state
	 */
	uint16_t sizes[5] = {
		DATASIZE - sizeof(struct verobj_read_write) - 2,
		DATASIZE - sizeof(struct verobj_read_write),
		DATASIZE,
		2*DATASIZE - sizeof(struct verobj_read_write),
		2*DATASIZE
	};
	static int counter = 0;

	counter = (counter + 1) % 5;
	return sizes[counter];
}

void NetSim::prep_write_buffer(char data, int size, uint16_t identifier)
{
	assert(size <= 2*DATASIZE);
	for (int i = 0; i < size; i++)
		write_data_buffer[i] = data;
	memcpy(write_data_buffer, &identifier, sizeof(uint16_t));
}

void NetSim::print_data(vector<char> data)
{
	for (int i = data.size() - 1; i >= 0; i--)
		printf("%02X", data[i] & 0xff);
	printf("\n");
}

bool NetSim::compare_data(vector<char> first, vector<char> second)
{
	if (first.size() != second.size())
		return false;

	for (int i = 0; i < first.size(); i++) {
		if (first[i] != second[i])
			return false;
	}
	return true;
}
