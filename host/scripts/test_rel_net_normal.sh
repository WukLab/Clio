#!/bin/bash
#
# Run test/test_rel_net_normal.c
# Mode 1: between host and board
# Mode 2: between host and monitor
#

set -x
set -e

# Mode 1
if [ 1 ]; then

./host.o --monitor=127.0.0.1:8888 --skip_join \
	 --port=1234 --dev=p4p1 \
	 --run_test=relnet_normal --add_board="192.168.1.23:1234"

else

# Mode 2
# monitor side
./monitor.o --dev ens4 --port 8888

# host side
./host.o -m 192.168.1.5:8888 -p 7777 -d p4p1  --run_test=relnet_normal

fi
