#!/bin/bash
# For test/test_migration.c

set -x

if [ "$1" == "1" ]; then
	./monitor.o -d lo -p 9999 --net_raw_ops=raw_udp
elif [ "$1" == "2" ]; then
	# We need at least board instances to test migration.
	./board_emulator.o -d lo -p 9998 --net_raw_ops=raw_udp -m 127.0.0.1:9999 &

	./board_emulator.o -d lo -p 9997 --net_raw_ops=raw_udp -m 127.0.0.1:9999
elif [ "$1" == "3" ]; then
	./host.o -d lo -p 7777 -m 127.0.0.1:9999  --run_test --net_raw_ops=raw_udp
elif [ "$1" == "4" ]; then
	pkill -f monitor.o
	pkill -f host.o
	pkill -f board_emulator.o
fi
