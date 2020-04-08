#!/bin/bash

# This script shows how to run test_raw_net.c
#
# This test case is not straightforward to setup because the current stack
# is already a bit complex and integrated with the GBN. Nontherless, we have
# added enough tuning options to make it possible to test via command line options.
#
# 1. The requirement is that we must skip the GBN stack by passing:
#	`--net_trans_ops=bypass`
# 2. Using any of the raw net ops is fine.
# 3. For host side, we need to pass `--skip_join` otherwise host will send msg
#    to host and wait for reply. This won't work because we need to further disable
#    host side mgmt polling thread. We cannot have another caller use `raw_net_receive`.
# 4. Last, we need to change the code a bit to disable the polling thread,
#    add this line to host's dispatcher():
#	`while (1) ;`
#
# The following two lines show a possible setup:
# - At 02, run monitor.
# - At 05, run host.

./monitor.o --dev p4p1 --port=8888 --net_trans_ops=bypass --net_raw_ops=raw_verbs
./host.o -m 192.168.1.2:8888 -p 8880 -d ens4 --skip_join --net_trans_ops=bypass --net_raw_ops=raw_verbs --run_test 
