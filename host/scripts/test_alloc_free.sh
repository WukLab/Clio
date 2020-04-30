#!/bin/bash
#
# Run test/test_alloc
#
# You must have started a monitor, a board, and this host.
# Because the board will not join the cluster at this point.
# we need to manually add it, changed IP for your testing.

set -x
set -e

board_ip="192.168.1.31:1234"

if [ "$1" == "1" ]; then
	./monitor.o -d p4p1 -p 8888 --add_board=$board_ip
elif [ "$1" == "2" ]; then
	./host.o --monitor=192.168.1.2:8888 \
		 --port=1234 --dev=p4p1 --net_trans_ops=gbn \
		 --run_test=alloc_free --add_board=$board_ip
fi
