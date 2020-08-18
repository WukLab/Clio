#!/bin/bash
set -e
set -x

TEST_NAME=pointer_chasing

# Change -m, -p, -d accordingly
./host.o -m 192.168.1.3:10000 -p 10000 -d ens4 --run_test=$TEST_NAME
