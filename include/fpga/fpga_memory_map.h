/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _FPGA_MEMORY_MAP_H_
#define _FPGA_MEMORY_MAP_H_

/*
 * Current legomem memory map:
 * 0-512M -> network cache
 * 512M - 1G -> pgtable
 * 1G - 2G -> data
 */

#define FPGA_MEMORY_MAP_DATA_START	(0x40000000)
#define FPGA_MEMORY_MAP_DATA_END	(0x80000000)

#endif /* _FPGA_MEMORY_MAP_H_ */
