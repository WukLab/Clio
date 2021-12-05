#!/bin/bash
set -e
set -x

NETDEV=p4p1
TEST_NAME=rw_threads

# TUNE ME during test
BOARD_IP=192.168.1.26:1234

# No monitor required!
sudo ./host.o -m 192.168.1.3:10000 --skip_join -p 10000 \
	-d $NETDEV \
	--run_test=$TEST_NAME \
	--add_board=$BOARD_IP

#--net_raw_ops=raw_socket \
