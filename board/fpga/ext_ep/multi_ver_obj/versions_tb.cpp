/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <list>
#include "multi_ver_obj_tb.h"

version_bram_test_suite::version_bram_test_suite(int latency)
	: version_bram_latency(latency),
	  objid_pattern(latency)
{}

int version_bram_test_suite::version_bram_main_loop()
{
	unsigned long total_req = 0;
	uint16_t version_idxs_simluated[OBJ_ARRAY_COUNT] = {0};
	std::list<std::vector<int> > pattern = objid_pattern.unlabeled_obj_to_boxes();
	hls::stream<struct version_bram_if> cmds("cmds"), data("data");
	std::queue<std::vector<struct version_bram_if> > expected_outcomes;
	std::queue<std::vector<int> > real_outcomes;
	std::vector<struct version_bram_if> expected_outcome;
	std::vector<int> real_outcome;
	int output_received = 0;
	int cycle_counter = 0;

	for (std::list<std::vector<int> >::iterator it = pattern.begin();
			it != pattern.end(); it++) {
		for (ap_uint<64> i = 0; i < (1 << version_bram_latency); i++) {
			total_req++;
			expected_outcome = feed_cmd(*it, i, cmds, version_idxs_simluated);
			expected_outcomes.push(expected_outcome);
		}
	}
	real_outcome.resize(version_bram_latency);

	while (cycle_counter < version_bram_latency * total_req * 2) {
		version_idxs2(cmds, data);
		if (!data.empty()) {
			real_outcome[output_received] = data.read().version;
			output_received++;
			if (output_received == version_bram_latency) {
				output_received = 0;
				real_outcomes.push(real_outcome);
			}
		}
		cycle_counter++;
	}

	/* check error */
	for (int i = 0; i < total_req; i++) {
		if (expected_outcomes.empty()) {
			std::cout << "implementation error\n";
		}
		if (real_outcomes.empty()) {
			std::cout << "didn't receive as mannge request as sent, sent: "
				<< total_req * version_bram_latency
				<< " received: " << i * version_bram_latency << std::endl;
			return -3;
		}
		expected_outcome = expected_outcomes.front();
		expected_outcomes.pop();
		real_outcome = real_outcomes.front();
		real_outcomes.pop();

		for (int j = 0; j < version_bram_latency; j++) {
			if (real_outcome[j] == expected_outcome[j].version)
				continue;

			for (int k = 0; k < version_bram_latency; k++) {
				std::cout << "req number: " << i
					<< " obj_id: " << expected_outcome[k].obj_id
					<< " rw: " << expected_outcome[k].rw
					<< " expected: " << expected_outcome[k].version
					<< " real: " << real_outcome[k] << std::endl;
			}
			return -1;
		}
	}

	std::cout << "version index BRAM synthetic dependency test SUCCESS!!\n";
	std::cout << "total # of request done: " << total_req * version_bram_latency << "\n\n";
	return 0;
}

std::vector<struct version_bram_if>
version_bram_test_suite::feed_cmd(std::vector<int> &obj_id_pattern, ap_uint<64> rw_pattern,
		hls::stream<struct version_bram_if> &cmd_out, uint16_t* versions_idxs)
{
	struct version_bram_if cmd;
	std::vector<struct version_bram_if> expected_result;
	expected_result.resize(version_bram_latency);

	for (int i = 0; i < version_bram_latency; i++) {
		assert(OBJ_ARRAY_COUNT > cmd.obj_id);
		cmd.obj_id = obj_id_pattern[i];
		if (rw_pattern[i] == 0) {
			cmd.rw = VERSION_READ;
			expected_result[i].version = versions_idxs[cmd.obj_id];
		} else {
			cmd.rw = VERSION_INC;
			expected_result[i].version = ++versions_idxs[cmd.obj_id];
		}
		expected_result[i].rw = cmd.rw;
		expected_result[i].obj_id = cmd.obj_id;
		cmd.version = 0;
		cmd_out.write(cmd);
	}
	return expected_result;
}
