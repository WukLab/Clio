/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_PATTERN_GENERATOR_H_
#define _LEGO_MEM_PATTERN_GENERATOR_H_

#include <list>
#include <vector>

class Patterns {
public:
	Patterns(int pattern_len);
	std::list<std::vector<int> > unlabeled_obj_to_boxes();
	void print_pattern();
	
private:
	const int pattern_len;
	
	std::list<std::list<int> > possible_two_sum(int last, int sum_target);
	std::list<std::list<int> > possible_multi_sum(int box_count, int last, int obj_count);
};

#endif /* _LEGO_MEM_PATTERN_GENERATOR_H_ */
