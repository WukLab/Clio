/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * This file describes the bus address layout for ZCU106 SoC used in LegoMem.
 * Vivado Address Editor shows a rough layout. For details, see:
 * https://docs.google.com/spreadsheets/d/1mrXRYzTilt4WdN5SEQtEN4X9tKSQfa6ttvXFGojpjBs/edit#gid=0
 *
 * Those are SoC Bus Address. SoC can also access FPGA physical DRAM,
 * which is described by fpga_memory_map.h
 *
 * SoC Bus Address Layout:
 *
 * 	-------------------------   <--- 0x0
 * 	|			 |
 * 	| SoC Physical DRAM      |
 * 	|			 |
 * 	------------------------- 
 * 	|	misc		 |
 * 	-------------------------   <--- 0xA000C000
 * 	|			 |
 * 	|	regmap		 |
 * 	|			 |
 * 	------------------------- 
 * 	|			 |
 * 	|	holes		 |
 * 	|			 |
 * 	|			 |
 * 	|			 |
 * 	|			 |
 * 	-------------------------  <--- 0x5_0000_0000 FPGA_MEMORY_MAP_MAPPING_BASE
 * 	|			 |
 * 	|			 |	This soc bus address space completely maps
 * 	|			 |	to fpga physical DRAM. See (fpga_memory_map.h)
 * 	|			 |
 * 	|			 |
 * 	|			 |
 * 	|			 |
 * 	|			 |
 * 	-------------------------
 *
 */

#ifndef _LEGOMEM_FPGA_ZYNQ_REGMAP_
#define _LEGOMEM_FPGA_ZYNQ_REGMAP_

/*
 * MM Register Properties
 * WO: Write only
 * RO: Read only
 * RW: Read/Write
 */
#define SOC_REGMAP_START			(0xA000C000UL)
#define SOC_REGMAP_NET_SFP_IP			(0xA000C000UL) /* WO */
#define SOC_REGMAP_NET_SFP_MAC_LO		(0xA000C004UL) /* WO mac [31:0] */
#define SOC_REGMAP_NET_SFP_MAC_HI		(0xA000C008UL) /* WO mac [47:32] */

#define SOC_REGMAP_STAT_START			(0xA0006000UL)
#define SOC_REGMAP_STAT_END			(0xA0006FFFUL)

#endif /* _LEGOMEM_FPGA_ZYNQ_REGMAP_ */
