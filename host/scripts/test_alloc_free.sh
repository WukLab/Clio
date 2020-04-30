#!/bin/bash
#
# Run test/test_alloc
#
# You must have started a monitor, a board, and this host.
# Because the board will not join the cluster at this point.
# we need to manually add it. Change IP for your testing.
#
# My testing model
#    - run both monitor and host on wuklab02
#    $ ./script/test_alloc_free.sh 1
#    $ ./script/test_alloc_free.sh 2
#

set -x
set -e

# Knob
# Make sure board is already up and running
board_ip="192.168.1.31:1234"

if [ "$1" == "1" ]; then
	./monitor.o \
		--dev=p4p1 \
		--port=8888 \
		--add_board=$board_ip
elif [ "$1" == "2" ]; then
	./host.o \
		--monitor=192.168.1.2:8888 \
	 	--dev=p4p1 \
		--port=1234 \
		--net_trans_ops=gbn \
		--run_test=alloc_free
fi
