/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_CTRL_
#define _LEGO_MEM_CTRL_

/*
 * CMDs for lego_mem_ctrl->cmd
 */
enum {
	CMD_LEGOMEM_CTRL_CREATE_PROC,
	CMD_LEGOMEM_CTRL_ALLOC,
	CMD_LEGOMEM_CTRL_FREE,
};

struct __attribute__((__packed__)) lego_mem_pte {
    uint64_t ppa : 60;
    uint8_t  page_type : 2;
    uint8_t  allocated : 1;
    uint8_t  valid : 1;
    uint64_t tag;
};

struct __attribute__((__packed__)) lego_mem_ctrl {
    uint32_t param32;
    uint8_t param8;
    uint8_t beats : 4;
    uint8_t cmd : 4;
    uint8_t addr;
    uint8_t epid;
};

#endif
