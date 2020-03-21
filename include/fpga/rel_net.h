/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_REL_NET_H_
#define _LEGO_MEM_REL_NET_H_

#define WINDOW_SIZE		256
#define MAX_NR_CONN		(1024)

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

#define RETRANS_TIMEOUT_CYCLE	100000000

enum pkt_type {
	pkt_type_ack = 1,
	pkt_type_nack = 2,
	pkt_type_data = 3,
	pkt_type_syn,
	pkt_type_fin
};

#endif
