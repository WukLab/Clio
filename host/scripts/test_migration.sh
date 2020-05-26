#!/bin/bash
# For test/test_migration.c

set -x

board_ip="192.168.1.9:1234"

if [ "$1" == "1" ]; then
	./monitor.o -d ens4 -p 1234 --add_board=$board_ip
elif [ "$1" == "2" ]; then
	#
	# We need at least two board instances to test migration.
	#
	# We can skip this if we have two real boards
	#
	./board_emulator.o -d lo -p 9998 -m 127.0.0.1:9999 &
	./board_emulator.o -d lo -p 9997 -m 127.0.0.1:9999
elif [ "$1" == "3" ]; then
	./host.o \
		-d ens4 \
		-p 40000 \
		-m 192.168.1.2:20000 \
		--run_test=migration
elif [ "$1" == "4" ]; then
	pkill -f monitor.o
	pkill -f host.o
	pkill -f board_emulator.o
fi
