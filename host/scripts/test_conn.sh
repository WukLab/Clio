#!/bin/bash

set -x
set -e

# Make sure board is already up and running
board_ip="192.168.1.23:1234"

monitor_port=10000
monitor_ip="192.168.1.5"

if [ "$1" == "1" ]; then
	./monitor.o \
		--dev=p4p1 \
		--port=$monitor_port \
		--add_board=$board_ip
elif [ "$1" == "2" ]; then
	./host.o \
		--monitor=$monitor_ip:$monitor_port \
	 	--dev=ens4 \
		--port=10000 \
		--run_test=rw_same
fi
