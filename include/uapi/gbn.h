/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_REL_NET_H_
#define _LEGO_MEM_REL_NET_H_

// fix segment size: 9kb
#define MAX_PACKET_SIZE		(9216)
#define WINDOW_SIZE_WIDTH	(7)
#define WINDOW_SIZE		(1 << (WINDOW_SIZE_WIDTH))
#define WINDOW_IDX_MSK		(WINDOW_SIZE - 1)
#define NR_MAX_SESSIONS_PER_NODE	(1024)

#define LEGOMEM_PORT		(1234)

/*
 * gbn header format:
 * | pkt_type | seqnum | ses_id |
 * |0	     7|8     39|40    63|
 */
#define PKT_TYPE_WIDTH		(8)
#define SEQ_SIZE_BYTE		(4)
#define SEQ_WIDTH		(32)
#define SES_ID_SIZE_BYTE	(3)
#define SES_ID_WIDTH		(24)

#define SEQ_OFFSET		(PKT_TYPE_WIDTH)
#define SES_ID_OFFSET		(PKT_TYPE_WIDTH + SEQ_WIDTH)

/*
 * session id format:
 * | src slot id | dest slot id |
 * |0		9|10	      19| (local offset)
 * |40	       49|50	      59| (global offset)
 */
#define SLOT_ID_WIDTH		(10)
#define SRC_SLOT_OFFSET		(SES_ID_OFFSET)
#define DEST_SLOT_OFFSET	(SES_ID_OFFSET + SLOT_ID_WIDTH)

/*
 * subnet: 192.168.1.0/24
 */
#define SUBNET			(0xc0a80100)
#define SUBNET_MASK		(0xffffff00)

/*
 * attention: don't make retrans_timeout_cycle less than 1
 */
#define RETRANS_TIMEOUT_US	(4000)
#define CYCLE_TIME_NS		(4)
#define RETRANS_TIMEOUT_CYCLE	(RETRANS_TIMEOUT_US * 1000 / CYCLE_TIME_NS)

// buff starts at 1GB
#define BUFF_ADDRESS_START	(0x40000000)

enum gbn_pkt_type {
	GBN_PKT_ACK = 1,
	GBN_PKT_NACK = 2,
	GBN_PKT_DATA = 3,
};

#endif
