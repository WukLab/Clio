/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * This file describes the memory-mapped register layout of legomem zynq.
 * Vivado Address Editor shows a rough layout. For details, see:
 * https://docs.google.com/spreadsheets/d/1mrXRYzTilt4WdN5SEQtEN4X9tKSQfa6ttvXFGojpjBs/edit#gid=0
 */

#ifndef _LEGOMEM_FPGA_ZYNQ_REGMAP_
#define _LEGOMEM_FPGA_ZYNQ_REGMAP_

/*
 * MM Register Properties
 * WO: Write only
 * RO: Read only
 * RW: Read/Write
 */

#define SOC_REGMAP_NET_SFP_IP			(0xA000C000) /* WO */
#define SOC_REGMAP_NET_SFP_MAC_LO		(0xA000C004) /* WO mac [31:0] */
#define SOC_REGMAP_NET_SFP_MAC_HI		(0xA000C008) /* WO mac [47:32] */

#endif /* _LEGOMEM_FPGA_ZYNQ_REGMAP_ */
