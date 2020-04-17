#!/bin/bash

set -x
set -e

#
# The tests run between a host and a monitor,
# the file is test_rel_net.c
#
# The test commands are nothing special but normal args.
#

# monitor side
./monitor.o --dev ens4 --port 8888

# host side
./host.o -m 192.168.1.5:8888 -p 7777 -d p4p1  --run_test
