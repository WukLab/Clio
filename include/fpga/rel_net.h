/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_REL_NET_H_
#define _LEGO_MEM_REL_NET_H_

/**
 * to run hls c-simulation, define DEBUG_MODE here
 */
#define DEBUG_MODE_

#include <ap_int.h>

/**
 * ? what's the max packet size?
 * a reasonable guess: pagesize(4K) + lego_header(8 byte)
 * every transfer is 64 bit, thus ÷8
 */
#define MAX_PACKET_SIZE		4104 / 8
#define WINDOW_INDEX_MSK	0x07
#define WINDOW_SIZE		8

#ifdef DEBUG_MODE
 #define TIMEOUT		100
#else
 #define TIMEOUT		100000000
#endif

enum pkt_type {
	pkt_type_ack = 1,
	pkt_type_nack = 2,
	pkt_type_data = 3
};

#endif
