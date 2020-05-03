/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_FIFO_H_
#define _LEGO_MEM_FIFO_H_

#include <hls_stream.h>

template <typename T>
class FIFO {

protected:
	T top;
	bool top_vld;
	hls::stream<T> rest;

public:
	FIFO() {
		top = 0;
		top_vld = 0;
	}

	bool empty() {
		return top_vld == false;
	}

	T front() {
		if (top_vld == 0)
			return static_cast<T>(0);

		return top;
	}

	// assume user check empty before pop
	T pop() {
		T top_entry = top;
		if (!rest.empty())
			top = rest.read();
		else
			top_vld = false;
		return top_entry;
	}

	void push(T input) {
		if (empty()) {
			top = input;
			top_vld = true;
		} else {
			rest.write(input);
		}
	}
};

#endif /* _LEGO_MEM_FIFO_H_ */
