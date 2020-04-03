#!/bin/bash

set -x
set -e

# Single board, no monitor setting

# Note
# 1) You don't need to start a monitor, the address below is just for setup purpose.
# 2) --add_board needs to be the real board's IP and port, make sure it's running beforehand.
#    Also make sure the board has ARP protocol up and running.
# 3) Use the --dev accordingly, make sure you use the Mellanox NIC.
#    Usually it is p4p1 or ens4
# 4) At this point (Mar 28), you have to use `--add_board` to manually add a remote board.
#    Because the board has no join_cluster feature.
# 5) we now only supports 1 added board. It's possible to have more. Upon request.
#
# And without monitor, you can not use the following legomem APIs because they will
# try to contact monitor by default:
# - legomem_open/close_context
# - legomem_alloc/free

./host.o --monitor=127.0.0.1:8888 --skip_join --port=1234 --dev=p4p1 --run_test --add_board="192.168.1.128:1234"
