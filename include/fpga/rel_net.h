/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_REL_NET_H_
#define _LEGO_MEM_REL_NET_H_

/**
 * to run hls c-simulation, define DEBUG_MODE here
 */
//#define DEBUG_MODE

#include <ap_int.h>

/**
 * ? what's the max packet size?
 * a reasonable guess: pagesize(4K) + lego_header(8 byte)
 * every transfer is 64 bit, thus ÷8
 */
#define MAX_PACKET_SIZE		(4096 / 8)
#define WINDOW_INDEX_MSK	0x07
#define WINDOW_SIZE		8

#ifdef DEBUG_MODE
 #define RETRANS_TIMEOUT_CYCLE		100
#else
 #define RETRANS_TIMEOUT_CYCLE		100000000
#endif

#endif
