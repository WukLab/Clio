#!/bin/bash
set -e
set -x

BOARD_IP=192.168.255.4:1234

sudo ./host.o -m 192.168.1.2:20000 --skip_join -p 10000 -d ens5f0 --run_test=kvs_ycsb --add_board=$BOARD_IP
