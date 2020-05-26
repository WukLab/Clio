#!/bin/bash
set -e
set -x

./host.o -m 192.168.1.2:20000 -p 10000 -d ens4 --run_test=kvs_simple
