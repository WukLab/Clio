#!/bin/bash

set -x
set -e

if [ "$1" == "1" ]; then
	./monitor.o -d lo -p 9999 --net_raw_ops=raw_udp
elif [ "$1" == "2" ]; then
	./board_emulator.o -d lo -p 9998 --net_raw_ops=raw_udp -m 127.0.0.1:9999
elif [ "$1" == "3" ]; then
	./host.o -d lo -p 7777 -m 127.0.0.1:9999  --run_test --net_raw_ops=raw_udp
fi
