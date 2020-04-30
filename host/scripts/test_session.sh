#!/bin/bash
#
# Run test/test_session.c
# Mode 1: between host and board
# Mode 2: between host and monitor
#

set -x
set -e

# Mode 1
if [ 1 ]; then
./host.o --monitor=127.0.0.1:8888 --skip_join \
	 --port=1234 --dev=p4p1 --net_trans_ops=gbn \
	 --run_test=session --add_board="192.168.1.31:1234"

else

# Mode 2
# You also need to start a monitor
./host.o --monitor=127.0.0.1:8888 \
	 --port=1234 --dev=p4p1 --net_trans_ops=gbn \
	 --run_test=session
fi
