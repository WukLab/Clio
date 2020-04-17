#!/bin/bash

set -x
set -e

#
# Test normal reliable network session between 2 hosts.
# 0. change main_host.c to use test_rel_net_normal().
#    and make sure one is server and one is client.
# 1. no monitor
# 2. two host.o instances running at 2 different machines (loopback is also possibke)
#

./host.o -m 127.0.0.1:1 --skip_join -p 8888 -d p4p1 --run_test --add_board=192.168.1.5:8888

./host.o -m 127.0.0.1:1 --skip_join -p 8888 -d ens4 --run_test --add_board=192.168.1.5:8888
