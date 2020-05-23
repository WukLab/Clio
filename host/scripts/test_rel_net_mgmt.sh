#!/bin/bash

set -x
set -e

#
# The tests run between a host and a monitor,
# the file is test_rel_net_mgmt.c
#
# The test commands are nothing special but normal args.
#
#
# REMEMBER to change dev name and ports accordingly
#

# monitor side
#./monitor.o --dev ens4 --port 8888

#
# FAT NOTE
#
# If you are testing against a real board,
# you do NOT need to start soc code.
#
./host.o --monitor=127.0.0.1:8888 --skip_join \
	 --port=8888 --dev=p4p1 \
	 --run_test=relnet_mgmt --add_board="192.168.1.23:1234"
