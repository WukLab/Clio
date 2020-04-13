/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include "ext_ep.h"

#if WAITQUEUEVER == 1

WaitQueue::WaitQueue()
	: size(WAITQUEUESIZE)
{
	front_idx = 0;
	count = 0;
}

void WaitQueue::idxadd()
{
	front_idx++;
	front_idx %= size;
}

unsigned int WaitQueue::back_idx()
{
	// inclusive
	unsigned int idx = front_idx + count - 1;
	return idx %= size;
}

bool WaitQueue::empty()
{
	return count == 0;
}

bool WaitQueue::full()
{
	return count == size;
}

uint8_t WaitQueue::front()
{
	return data[front_idx];
}

uint8_t WaitQueue::pop()
{
	uint8_t entry = front();
	idxadd();
	count--;
	return entry;
}

int WaitQueue::push(uint8_t input)
{
	if (full())
		return -1;

	count++;
	data[back_idx()] = input;
	return 0;
}

#endif
