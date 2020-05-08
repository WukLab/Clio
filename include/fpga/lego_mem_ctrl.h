/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _LEGO_MEM_CTRL_
#define _LEGO_MEM_CTRL_

#include <uapi/compiler.h>
#include <fpga/pgtable.h>

/*
 * CMDs for lego_mem_ctrl->cmd
 */
enum {
	CMD_LEGOMEM_CTRL_CREATE_PROC,
	CMD_LEGOMEM_CTRL_ALLOC,
	CMD_LEGOMEM_CTRL_FREE,
};

struct lego_mem_ctrl {
    uint32_t param32;
    uint8_t param8;
    uint8_t beats : 4;
    uint8_t cmd : 4;
    uint8_t addr;
    uint8_t epid;
} __packed;

#endif
