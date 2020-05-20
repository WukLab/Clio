/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_MULTI_VER_OBJ_BRAMDATA_H_
#define _LEGO_MEM_MULTI_VER_OBJ_BRAMDATA_H_

#ifdef __SYNTHESIZE__
#define NDEBUG
#endif

#include <cassert>
#include "multi_ver_obj.h"

class multi_ver_obj_data {
public:

	uint16_t pid;
	uint64_t objarray_ptr;
	uint64_t freelist_ptr;
	uint64_t freelist_head;
	uint64_t freelist_tail;		// inclusive
	/*
	 * this is used for first round of allocation before freelist
	 * established in DRAM
	 */
	uint32_t firstround_cnt;
	bool     freelist_full;
	bool     freelist_empty;

	multi_ver_obj_data() {
		pid = 0;
		objarray_ptr = 0;
		freelist_ptr = 0;
		freelist_head = 0;
		freelist_tail = 0;
		firstround_cnt = OBJ_ARRAY_COUNT;
		freelist_full = false;
		freelist_empty = true;
	}

	bool empty() {
		return firstround_cnt == 0 && freelist_empty;
	}

	bool is_firstround() {
		return firstround_cnt != 0;
	}

	uint32_t frontptr() {
		/* use shift instead of multiplication to reduce latency */
		return freelist_ptr + freelist_head << LOG_IDX_SIZE;
	}

	uint32_t tailptr() {
		/* use shift instead of multiplication to reduce latency */
		return freelist_ptr + freelist_tail << LOG_IDX_SIZE;
	}

	void tail_advance() {
		assert(!freelist_full);

		if (freelist_empty) {
			freelist_empty = false;
		} else {
			if (freelist_tail == OBJ_ARRAY_COUNT - 1)
				freelist_tail = 0;
			else
				freelist_tail++;
			if (freelist_tail == freelist_head)
				freelist_full = true;
		}
	}

	/* assume firstround_cnt == 0 */
	void head_advance() {
		assert(!is_firstround());
		assert(!freelist_empty);

		if (freelist_head == OBJ_ARRAY_COUNT - 1)
			freelist_head = 0;
		else
			freelist_head++;
		if (freelist_tail == freelist_head)
			freelist_empty = true;
		freelist_full = false;
	}


	/* assume firstround_cnt != 0 */
	void head_advance_firstround() {
		assert(is_firstround());

		firstround_cnt--;
	}
};

struct verobj_data_copy {
	uint16_t pid;
	uint64_t objarray_ptr;
	uint64_t freelist_ptr;
};
boundary1(verobj_data, verobj_data_copy, pid)
boundary1(verobj_data, verobj_data_copy, objarray_ptr)
boundary1(verobj_data, verobj_data_copy, freelist_ptr)

#endif /* _LEGO_MEM_MULTI_VER_OBJ_BRAMDATA_H_ */
