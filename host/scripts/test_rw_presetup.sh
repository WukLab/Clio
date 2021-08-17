#!/bin/bash
set -e
set -x

TEST_NAME=rw_presetup

# TUNE ME during test
BOARD_IP=192.168.255.4:1234
# BOARD_IP=192.168.254.6:1234

# No monitor required!
sudo ./host.o -m 192.168.1.3:10000 --skip_join -p 10000 -d ens5f0 --run_test=$TEST_NAME --add_board=$BOARD_IP
