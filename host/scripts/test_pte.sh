#!/bin/bash

./host.o --monitor=127.0.0.1:8888 --skip_join \
	 --port=1234 --dev=p4p1 \
	 --run_test=test_pte --add_board="192.168.1.31:1234"
