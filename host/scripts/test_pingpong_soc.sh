#!/bin/bash
# Run test/test_pingpong_soc.c

set -x
set -e

./host.o --monitor=127.0.0.1:8888 --skip_join \
	 --net_trans_ops=gbn \
	 --run_test=pingpong_soc \
	 --port=1234 \
	 --dev=p4p1 \
	 --add_board="192.168.1.31:1234"
