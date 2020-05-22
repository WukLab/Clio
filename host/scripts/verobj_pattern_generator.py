#!/usr/bin/python3

import numpy as np
import matplotlib.pyplot as plt

#
# Used for test/test_multiver_obj.c
# REQUEST_COUNT:	number of read/write request in total
# READ_PRECENT:		0 - 100, WRITE_PRECENT = (1 - READ_PRECENT)
#

REQUEST_COUNT = 1000
READ_PRECENT = 50

def req_generate(filename):
	bernoulli_p = READ_PRECENT / 100.
	alpha = 2.0
	zipfRV = None
	while zipfRV is None or np.max(zipfRV) >= REQUEST_COUNT:
		zipfRV = np.random.default_rng().zipf(alpha, REQUEST_COUNT)
		# zipf starts from 1, we want 0
		zipfRV = zipfRV - 1
	binRV = np.random.default_rng().choice(2, REQUEST_COUNT, p=[bernoulli_p, 1-bernoulli_p])

	string = ''
	for objid, rw in zip(zipfRV, binRV):
		string += '{}\t{}\n'.format(objid, rw)

	with open(filename, 'w') as fd:
		fd.write(string)

	print("request generation done")

if __name__ == "__main__":
	 req_generate("req_pattern.log")
