/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

/*
 * All header size macros are in BYTES.
 */

#ifndef _UAPI_NET_HEADER_H_
#define _UAPI_NET_HEADER_H_

#include <fpga/rel_net.h>

struct ethernet_header {
	char	mac_dst[6];
	char	mac_src[6];
	char	ethtype[2];
} __attribute__((packed));

struct ip_header {
	char	word0[4];
	char	word1[4];
	char	word2[4];
	char	word3[4];
	char	word4[4];
} __attribute__((packed));

struct udp_header {
	char	port_src[2];
	char	port_dst[2];
	char	length[2];
	char	checksum[2];
} __attribute__((packed));

struct lego_header {
	char	app_id;
	int	seq;
} __attribute__((packed));

struct gbn_header {
	char		type;
	unsigned int	seqnum;
	char		session_id[7-SEQ_SIZE_BYTE];
} __attribute__((packed));

#define ETHERNET_HEADER_SIZE	(14)
#define IP_HEADER_SIZE		(20)
#define UDP_HEADER_SIZE		(8)
#define GBN_HEADER_SIZE		(sizeof(struct gbn_header))
#define LEGO_HEADER_SIZE	(sizeof(struct lego_header))

#define GBN_HEADER_OFFSET \
	(ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + UDP_HEADER_SIZE)
#define LEGO_HEADER_OFFSET \
	(GBN_HEADER_OFFSET + GBN_HEADER_SIZE)

#endif /* _UAPI_NET_HEADER_H_ */
