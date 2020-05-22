#!/bin/bash
#
# Run test/test_read_write
#
# You must have started a monitor, a board, and this host.
# Because the board will not join the cluster at this point.
# we need to manually add it. Change IP for your testing.
#
# testing model
#    $ ./script/test_alloc_free.sh 1   e.g., on wuklab03
#    $ ./script/test_alloc_free.sh 2   e.g., on wuklab02
#

set -x
set -e

# Knob
# Make sure board is already up and running
board_ip="192.168.1.23:1234"

monitor_port=1234
monitor_ip="192.168.1.2"

if [ "$1" == "1" ]; then
	if true; then
	#if false; then
		./monitor.o \
			--dev=p4p1 \
			--port=$monitor_port \
			--add_board=192.168.1.23:1234 \
			--add_board=192.168.1.22:1234
	else
		./monitor.o \
			--dev=p4p1 \
			--port=$monitor_port
	fi
elif [ "$1" == "2" ]; then
	./host.o \
		--monitor=$monitor_ip:$monitor_port \
	 	--dev=ens4 \
		--port=1234 \
		--run_test=rw_same

		#--run_test=rw_inline
		#--run_test=rw_fault
		#--run_test=rw_tlb
		#--run_test=rw_same

elif [ "$1" == "3" ]; then
	./board_emulator.o \
		--monitor=$monitor_ip:$monitor_port \
		--d=p4p1 \
		--port=9998
fi
