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
board_ip="192.168.1.23:1234"

monitor_port=1234
monitor_ip="192.168.1.3"

if [ "$1" == "1" ]; then
	if true; then
	#if false; then
		./monitor.o \
			--dev=p4p1 \
			--port=$monitor_port \
			--add_board=$board_ip
	else
		./monitor.o \
			--dev=p4p1 \
			--port=$monitor_port
	fi
elif [ "$1" == "2" ]; then
	./host.o \
		--monitor=$monitor_ip:$monitor_port \
	 	--dev=p4p1 \
		--port=1234 \
		--run_test=alloc_free
elif [ "$1" == "3" ]; then
	./board_emulator.o \
		--monitor=$monitor_ip:$monitor_port \
		--d=p4p1 \
		--port=9998
fi
