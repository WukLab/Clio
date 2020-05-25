#!/bin/bash
set -e
set -x

./host.o -m 192.168.1.2:7777 -p 8888 -d ens4 --run_test=kvs_simple
