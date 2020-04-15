/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_EXT_EP_API_H_
#define _LEGO_MEM_EXT_EP_API_H_

#include <stdint.h>

// TODO: use include for following definition after integration
// Lego mem request type definations
#define LEGOMEM_REQ_INVALID    (0x00)
#define LEGOMEM_REQ_READ       (0x01)
#define LEGOMEM_REQ_READ_RESP  (0x02)
#define LEGOMEM_REQ_WRITE      (0x03)
#define LEGOMEM_REQ_WRITE_RESP (0x04)
#define LEGOMEM_REQ_ALLOC      (0x05)
#define LEGOMEM_REQ_ALLOC_RESP (0x06)
#define LEGOMEM_REQ_FREE       (0x07)
#define LEGOMEM_REQ_FREE_RESP  (0x08)
// TODO: add to global header, new opcode
#define LEGOMEM_REQ_IREAD      (0x09)
#define LEGOMEM_REQ_IREAD_RESP (0x0A)
#define LEGOMEM_REQ_IWRITE     (0x0B)
#define LEGOMEM_REQ_IWRITE_RESP (0x0C)
// internal mgnt API, designed to be sent from SoC
#define LEGOMEM_REQ_CACHE_SHOOTDOWN (0xF0)

#define LEGOMEM_STATUS_OKAY            (0x00)
#define LEGOMEM_STATUS_ERR_INVALID     (0x01)
#define LEGOMEM_STATUS_ERR_WRITE_PERM  (0x02)
#define LEGOMEM_STATUS_ERR_READ_PERM   (0x03)

#define LEGOMEM_CONT_NONE   (0)
#define LEGOMEM_CONT_NET    (0)
#define LEGOMEM_CONT_MEM    (1)
#define LEGOMEM_CONT_SOC    (2)
// TODO: add to global header, new cont
#define LEGOMEM_CONT_EXTAPI (3)

// APIs is defined as from low to high, little endian
struct __attribute__((__packed__)) lego_mem_header {
	uint16_t		pid;
	uint8_t     		tag;
	uint8_t     		req_type;
	uint8_t     		req_status : 4;
	uint8_t     		flag_route : 1;
	uint8_t     		flag_repl : 1;
	uint8_t     		access_cnt : 1; // used by pointer dereference
	uint8_t     		reserved : 1;
	uint8_t     		seqId;
	uint16_t    		size;
	
	// Runtime information for on board end points
	// For normal commands, user do not fill this information
	uint16_t    		cont;
	uint16_t    		dest_port;
	uint32_t    		dest_ip;
};

struct op_read_write {
	uint64_t		va;
	uint64_t		size;

	/* Hold write data, variable length */
	char			data[0];
};

struct legomem_read_write_req {
	struct lego_mem_header 	hdr;
	struct op_read_write 	op;

	/* Hold write data, variable length */
	char			data[0];
};

struct op_deref {
	uint64_t		addr;
	uint32_t 		off1;	// offset of indirect address
	uint32_t 		off2;	// offset of address
	uint64_t		size;

	/* Hold write data, variable length */
	char			data[0];
};

struct legomem_deref_req {
	struct lego_mem_header 	hdr;
	struct op_deref 	op;
};

#endif /* _LEGO_MEM_EXT_EP_API_H_ */
