/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_H_
#define _LEGO_MEM_H_

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

// APIs is defined as from low to high, little endian
struct __attribute__((__packed__)) lego_mem_header {
    uint16_t    pid;
    uint8_t     tag;
    uint8_t     req_type;
    uint8_t     req_status : 4;
    uint8_t     flag_route : 1;
    uint8_t     flag_repl : 1;
    uint8_t     reserved : 2;
    uint8_t     seqId;
    uint16_t    size;

    // Runtime infomation for on board end points
    // For normal commands, user do not fill this information
    uint16_t    cont;
    uint16_t    dest_port;
    uint32_t    dest_ip;
};

struct __attribute__((__packed__)) lego_mem_access_header {
    struct lego_mem_header header;
    uint32_t    length;
    uint64_t    va;
};

// These headers are carefully aligned, make sure thers no struct packing
#define LEGOMEM_HEADER_SIZE         (sizeof(struct lego_mem_header))
#define LEGOMEM_ACCESS_HEADER_SIZE  (sizeof(struct lego_mem_access_header))

// Utils
#define MAKE_CONT(c1,c2,c3,c4)      (((c1) & 0xF) | (((c2) & 0xF) << 4) | (((c3) & 0xF) << 8) | (((c4) & 0xF) << 12))

#endif
