/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <iostream>
#include <pattern_generator.h>

using namespace std;

Patterns::Patterns(int pattern_len)
	: pattern_len(pattern_len)
{}

list<vector<int> > Patterns::unlabeled_obj_to_boxes()
{
	std::list<std::vector<int> > all_posibilities;
	for (int a_pattern = 1; a_pattern <= pattern_len; a_pattern++) {
		if (a_pattern == 1) {
			std::vector<int> a_result;
			for (int i = 0; i < pattern_len; i++)
				a_result.push_back(0);
			all_posibilities.push_back(a_result);
		} else {
			std::list<std::list<int> > results = possible_multi_sum(a_pattern, 1, pattern_len);
			for (std::list<std::list<int> >::iterator it = results.begin();
					it != results.end(); it++) {
				std::vector<int> a_result;
				int obj_id = 0;
				for (std::list<int>::iterator it2 = it->begin();
						it2 != it->end(); it2++) {
					for (int i = 0; i < (*it2); i++)
						a_result.push_back(obj_id);
					obj_id++;
				}
				all_posibilities.push_back(a_result);
			}
		}
	}
	return all_posibilities;
}

void Patterns::print_pattern()
{
	std::list<std::vector<int> > test_return;

	test_return = unlabeled_obj_to_boxes();
	for (std::list<std::vector<int> >::iterator it = test_return.begin();
			it != test_return.end(); it++) {
		for (std::vector<int>::iterator it2 = it->begin();
				it2 != it->end(); it2++) {
			std::cout << *it2 << " ";
		}
		std::cout << std::endl;
	}
}

list<list<int> > Patterns::possible_multi_sum(int addend_count, int last, int obj_count)
{
	list<list<int> > results, first_result;

	first_result = possible_two_sum(last, obj_count);
	if (addend_count > 2) {
		for (list<list<int> >::iterator it = first_result.begin();
				it != first_result.end(); it++) {
			int last;
			int sum_target = it->back();
			it->pop_back();
			last = it->back();

			list<list<int> > sub_results = possible_multi_sum(addend_count-1, last, sum_target);
			for (list<list<int> >::iterator it2 = sub_results.begin();
					it2 != sub_results.end(); it2++) {
				list<int> a_result;
				list<int>::iterator it3 = a_result.begin();
				a_result.insert(it3, it->begin(), it->end());
				a_result.insert(it3, it2->begin(), it2->end());
				results.push_back(a_result);
			}
		}
		return results;
	} else {
		return first_result;
	}
}

list<list<int> > Patterns::possible_two_sum(int last, int sum_target)
{
	int addend = last;
	list<list<int> > results;
	while (addend <= sum_target - addend) {
		list<int> a_result;
		a_result.push_back(addend);
		a_result.push_back(sum_target - addend);
		results.push_back(a_result);
		addend++;
	}
	return results;
}
