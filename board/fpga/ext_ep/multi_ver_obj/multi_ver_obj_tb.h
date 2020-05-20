/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_MULTI_VER_OBJ_TB_H_
#define _LEGO_MEM_MULTI_VER_OBJ_TB_H_

#include <vector>
#include <pattern_generator.h>
#include <tb_sim.h>
#include "multi_ver_obj.h"

/* change ap_uint size if latency larger than 64 */
class version_bram_test_suite {
public:
	Patterns objid_pattern;

	version_bram_test_suite(int latency);
	int version_bram_main_loop();

private:
	const int version_bram_latency;

	std::vector<struct version_bram_if>
	feed_cmd(std::vector<int> &obj_id_pattern, ap_uint<64> rw_pattern,
		 hls::stream<struct version_bram_if> &cmd_out, uint16_t* versions_idxs);
};

#endif /* _LEGO_MEM_MULTI_VER_OBJ_TB_H_ */
