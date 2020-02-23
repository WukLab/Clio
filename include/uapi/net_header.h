/*
 * Copyright (c) 2020，Wuklab, UCSD.
 *
 * This file describes the network header related information and helpers.
 * Note that all header size macros are in BYTES.
 */

#ifndef _UAPI_NET_HEADER_H_
#define _UAPI_NET_HEADER_H_

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <uapi/compiler.h>
#include <uapi/opcode.h>

struct eth_hdr {
	uint8_t dst_mac[6];
	uint8_t src_mac[6];
	uint16_t eth_type;
} __attribute__((packed));

struct ipv4_hdr {
	uint8_t ihl : 4;
	uint8_t version : 4;

	uint8_t ecn : 2;
	uint8_t dscp : 6;

	uint16_t tot_len;
	uint16_t id;
	uint16_t frag_off;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t check;
	uint32_t src_ip;
	uint32_t dst_ip;
} __attribute__((packed));

struct udp_hdr {
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t len;
	uint16_t check;
} __attribute__((packed));

/*
 * This is our layer's header.
 * See uapi/opcode.h for opcode definitions.
 */
struct lego_hdr {
	uint16_t opcode;
} __attribute__((packed));

#define SEQ_SIZE_BYTE		(4)
#define SEQ_WIDTH		(SEQ_SIZE_BYTE * 8)

struct gbn_header {
	char		type;
	unsigned int	seqnum;
	char		_resv[7-SEQ_SIZE_BYTE];
} __attribute__((packed));

enum pkt_type {
	pkt_type_ack = 1,
	pkt_type_nack = 2,
	pkt_type_data = 3
};

#define ETHERNET_HEADER_SIZE	(14)
#define IP_HEADER_SIZE		(20)
#define UDP_HEADER_SIZE		(8)
#define GBN_HEADER_SIZE		(sizeof(struct gbn_header))
#define LEGO_HEADER_SIZE	(sizeof(struct lego_hdr))

#define GBN_HEADER_OFFSET \
	(ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + UDP_HEADER_SIZE)
#define LEGO_HEADER_OFFSET \
	(GBN_HEADER_OFFSET + GBN_HEADER_SIZE)

static inline void *get_op_struct(void *packet)
{
	return packet + LEGO_HEADER_OFFSET + LEGO_HEADER_SIZE;
}

static void prepare_eth_header(struct eth_hdr *hdr, unsigned char *src_mac,
			       unsigned char *dst_mac)
{
	memcpy(hdr->src_mac, src_mac, 6);
	memcpy(hdr->dst_mac, dst_mac, 6);
	hdr->eth_type = htons(0x0800);
}

/*
 * Format the IPv4 for UDP packet.
 * @data_size is the data payload in the UDP packet.
 */
static __always_inline void
prepare_ipv4_header(struct ipv4_hdr *hdr, uint32_t src_ip,
		    uint32_t dst_ip, uint16_t data_size)
{
	hdr->version = 4;
	hdr->ihl = 5;

	hdr->ecn = 0;
	hdr->dscp = 0;

	hdr->tot_len = htons(sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) +
			     data_size);
	hdr->id = 0;
	hdr->frag_off = htons(0x4000);
	hdr->ttl = 64;

	/* UDP */
	hdr->protocol = 0x11;

	hdr->src_ip = htonl(src_ip);
	hdr->dst_ip = htonl(dst_ip);

	hdr->check = 0;
}

static __always_inline void
prepare_udp_header(struct udp_hdr *hdr, uint16_t src_port,
		   uint16_t dst_port, unsigned int payload_size)
{
	hdr->src_port = htons(src_port);
	hdr->dst_port = htons(dst_port);
	hdr->len = htons(sizeof(struct udp_hdr) + payload_size);
	hdr->check = 0;
}

struct routing_info {
	struct eth_hdr eth;
	struct ipv4_hdr ipv4;
	struct udp_hdr udp;
} __attribute__((packed));

struct endpoint_info {
	unsigned char mac[6];
	uint32_t ip;
	uint16_t udp_port;
};

static __always_inline void
prepare_routing_info(struct routing_info *ri, struct endpoint_info *src,
		     struct endpoint_info *dst)
{
	prepare_eth_header(&ri->eth, src->mac, dst->mac);
	prepare_ipv4_header(&ri->ipv4, src->ip, dst->ip, 0);
	prepare_udp_header(&ri->udp, src->udp_port, dst->udp_port, 0);
}

/**
 * Compute the IPv4 header checksum efficiently.
 * iph: ipv4 header
 * ihl: length of header / 4
 */
static inline uint16_t ip_csum(const void *iph, unsigned int ihl)
{
        unsigned int sum;

        asm("  movl (%1), %0\n"
            "  subl $4, %2\n"
            "  jbe 2f\n"
            "  addl 4(%1), %0\n"
            "  adcl 8(%1), %0\n"
            "  adcl 12(%1), %0\n"
            "1: adcl 16(%1), %0\n"
            "  lea 4(%1), %1\n"
            "  decl %2\n"
            "  jne      1b\n"
            "  adcl $0, %0\n"
            "  movl %0, %2\n"
            "  shrl $16, %0\n"
            "  addw %w2, %w0\n"
            "  adcl $0, %0\n"
            "  notl %0\n"
            "2:"
        /* Since the input registers which are loaded with iph and ihl
           are modified, we must also specify them as outputs, or gcc
           will assume they contain their original values. */
            : "=r" (sum), "=r" (iph), "=r" (ihl)
            : "1" (iph), "2" (ihl)
            : "memory");
        return (uint16_t)sum;
}

#endif /* _UAPI_NET_HEADER_H_ */
