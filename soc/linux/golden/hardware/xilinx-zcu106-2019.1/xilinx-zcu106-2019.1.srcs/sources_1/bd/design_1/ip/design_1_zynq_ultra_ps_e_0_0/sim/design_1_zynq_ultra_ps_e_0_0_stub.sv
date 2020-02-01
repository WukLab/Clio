// (c) Copyright 1995-2019 Xilinx, Inc. All rights reserved.
// 
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
// 
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
// 
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
// 
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.
// 
// DO NOT MODIFY THIS FILE.

//-----------------------------------------------------------------------------
// Filename:    design_1_zynq_ultra_ps_e_0_0_stub.sv
// Description: This HDL file is intended to be used with Xilinx Vivado Simulator (XSIM) only.
//-----------------------------------------------------------------------------
`ifdef XILINX_SIMULATOR

`ifndef XILINX_SIMULATOR_BITASBOOL
`define XILINX_SIMULATOR_BITASBOOL
typedef bit bit_as_bool;
`endif

(* SC_MODULE_EXPORT *)
module design_1_zynq_ultra_ps_e_0_0 (
  input bit_as_bool maxihpm0_fpd_aclk,
  output bit [15 : 0] maxigp0_awid,
  output bit [39 : 0] maxigp0_awaddr,
  output bit [7 : 0] maxigp0_awlen,
  output bit [2 : 0] maxigp0_awsize,
  output bit [1 : 0] maxigp0_awburst,
  output bit_as_bool maxigp0_awlock,
  output bit [3 : 0] maxigp0_awcache,
  output bit [2 : 0] maxigp0_awprot,
  output bit_as_bool maxigp0_awvalid,
  output bit [15 : 0] maxigp0_awuser,
  input bit_as_bool maxigp0_awready,
  output bit [127 : 0] maxigp0_wdata,
  output bit [15 : 0] maxigp0_wstrb,
  output bit_as_bool maxigp0_wlast,
  output bit_as_bool maxigp0_wvalid,
  input bit_as_bool maxigp0_wready,
  input bit [15 : 0] maxigp0_bid,
  input bit [1 : 0] maxigp0_bresp,
  input bit_as_bool maxigp0_bvalid,
  output bit_as_bool maxigp0_bready,
  output bit [15 : 0] maxigp0_arid,
  output bit [39 : 0] maxigp0_araddr,
  output bit [7 : 0] maxigp0_arlen,
  output bit [2 : 0] maxigp0_arsize,
  output bit [1 : 0] maxigp0_arburst,
  output bit_as_bool maxigp0_arlock,
  output bit [3 : 0] maxigp0_arcache,
  output bit [2 : 0] maxigp0_arprot,
  output bit_as_bool maxigp0_arvalid,
  output bit [15 : 0] maxigp0_aruser,
  input bit_as_bool maxigp0_arready,
  input bit [15 : 0] maxigp0_rid,
  input bit [127 : 0] maxigp0_rdata,
  input bit [1 : 0] maxigp0_rresp,
  input bit_as_bool maxigp0_rlast,
  input bit_as_bool maxigp0_rvalid,
  output bit_as_bool maxigp0_rready,
  output bit [3 : 0] maxigp0_awqos,
  output bit [3 : 0] maxigp0_arqos,
  input bit_as_bool maxihpm1_fpd_aclk,
  output bit [15 : 0] maxigp1_awid,
  output bit [39 : 0] maxigp1_awaddr,
  output bit [7 : 0] maxigp1_awlen,
  output bit [2 : 0] maxigp1_awsize,
  output bit [1 : 0] maxigp1_awburst,
  output bit_as_bool maxigp1_awlock,
  output bit [3 : 0] maxigp1_awcache,
  output bit [2 : 0] maxigp1_awprot,
  output bit_as_bool maxigp1_awvalid,
  output bit [15 : 0] maxigp1_awuser,
  input bit_as_bool maxigp1_awready,
  output bit [127 : 0] maxigp1_wdata,
  output bit [15 : 0] maxigp1_wstrb,
  output bit_as_bool maxigp1_wlast,
  output bit_as_bool maxigp1_wvalid,
  input bit_as_bool maxigp1_wready,
  input bit [15 : 0] maxigp1_bid,
  input bit [1 : 0] maxigp1_bresp,
  input bit_as_bool maxigp1_bvalid,
  output bit_as_bool maxigp1_bready,
  output bit [15 : 0] maxigp1_arid,
  output bit [39 : 0] maxigp1_araddr,
  output bit [7 : 0] maxigp1_arlen,
  output bit [2 : 0] maxigp1_arsize,
  output bit [1 : 0] maxigp1_arburst,
  output bit_as_bool maxigp1_arlock,
  output bit [3 : 0] maxigp1_arcache,
  output bit [2 : 0] maxigp1_arprot,
  output bit_as_bool maxigp1_arvalid,
  output bit [15 : 0] maxigp1_aruser,
  input bit_as_bool maxigp1_arready,
  input bit [15 : 0] maxigp1_rid,
  input bit [127 : 0] maxigp1_rdata,
  input bit [1 : 0] maxigp1_rresp,
  input bit_as_bool maxigp1_rlast,
  input bit_as_bool maxigp1_rvalid,
  output bit_as_bool maxigp1_rready,
  output bit [3 : 0] maxigp1_awqos,
  output bit [3 : 0] maxigp1_arqos,
  input bit_as_bool saxihpc0_fpd_aclk,
  input bit_as_bool saxigp0_aruser,
  input bit_as_bool saxigp0_awuser,
  input bit [5 : 0] saxigp0_awid,
  input bit [48 : 0] saxigp0_awaddr,
  input bit [7 : 0] saxigp0_awlen,
  input bit [2 : 0] saxigp0_awsize,
  input bit [1 : 0] saxigp0_awburst,
  input bit_as_bool saxigp0_awlock,
  input bit [3 : 0] saxigp0_awcache,
  input bit [2 : 0] saxigp0_awprot,
  input bit_as_bool saxigp0_awvalid,
  output bit_as_bool saxigp0_awready,
  input bit [127 : 0] saxigp0_wdata,
  input bit [15 : 0] saxigp0_wstrb,
  input bit_as_bool saxigp0_wlast,
  input bit_as_bool saxigp0_wvalid,
  output bit_as_bool saxigp0_wready,
  output bit [5 : 0] saxigp0_bid,
  output bit [1 : 0] saxigp0_bresp,
  output bit_as_bool saxigp0_bvalid,
  input bit_as_bool saxigp0_bready,
  input bit [5 : 0] saxigp0_arid,
  input bit [48 : 0] saxigp0_araddr,
  input bit [7 : 0] saxigp0_arlen,
  input bit [2 : 0] saxigp0_arsize,
  input bit [1 : 0] saxigp0_arburst,
  input bit_as_bool saxigp0_arlock,
  input bit [3 : 0] saxigp0_arcache,
  input bit [2 : 0] saxigp0_arprot,
  input bit_as_bool saxigp0_arvalid,
  output bit_as_bool saxigp0_arready,
  output bit [5 : 0] saxigp0_rid,
  output bit [127 : 0] saxigp0_rdata,
  output bit [1 : 0] saxigp0_rresp,
  output bit_as_bool saxigp0_rlast,
  output bit_as_bool saxigp0_rvalid,
  input bit_as_bool saxigp0_rready,
  input bit [3 : 0] saxigp0_awqos,
  input bit [3 : 0] saxigp0_arqos,
  input bit_as_bool saxihp1_fpd_aclk,
  input bit_as_bool saxigp3_aruser,
  input bit_as_bool saxigp3_awuser,
  input bit [5 : 0] saxigp3_awid,
  input bit [48 : 0] saxigp3_awaddr,
  input bit [7 : 0] saxigp3_awlen,
  input bit [2 : 0] saxigp3_awsize,
  input bit [1 : 0] saxigp3_awburst,
  input bit_as_bool saxigp3_awlock,
  input bit [3 : 0] saxigp3_awcache,
  input bit [2 : 0] saxigp3_awprot,
  input bit_as_bool saxigp3_awvalid,
  output bit_as_bool saxigp3_awready,
  input bit [127 : 0] saxigp3_wdata,
  input bit [15 : 0] saxigp3_wstrb,
  input bit_as_bool saxigp3_wlast,
  input bit_as_bool saxigp3_wvalid,
  output bit_as_bool saxigp3_wready,
  output bit [5 : 0] saxigp3_bid,
  output bit [1 : 0] saxigp3_bresp,
  output bit_as_bool saxigp3_bvalid,
  input bit_as_bool saxigp3_bready,
  input bit [5 : 0] saxigp3_arid,
  input bit [48 : 0] saxigp3_araddr,
  input bit [7 : 0] saxigp3_arlen,
  input bit [2 : 0] saxigp3_arsize,
  input bit [1 : 0] saxigp3_arburst,
  input bit_as_bool saxigp3_arlock,
  input bit [3 : 0] saxigp3_arcache,
  input bit [2 : 0] saxigp3_arprot,
  input bit_as_bool saxigp3_arvalid,
  output bit_as_bool saxigp3_arready,
  output bit [5 : 0] saxigp3_rid,
  output bit [127 : 0] saxigp3_rdata,
  output bit [1 : 0] saxigp3_rresp,
  output bit_as_bool saxigp3_rlast,
  output bit_as_bool saxigp3_rvalid,
  input bit_as_bool saxigp3_rready,
  input bit [3 : 0] saxigp3_awqos,
  input bit [3 : 0] saxigp3_arqos,
  input bit_as_bool saxihp2_fpd_aclk,
  input bit_as_bool saxigp4_aruser,
  input bit_as_bool saxigp4_awuser,
  input bit [5 : 0] saxigp4_awid,
  input bit [48 : 0] saxigp4_awaddr,
  input bit [7 : 0] saxigp4_awlen,
  input bit [2 : 0] saxigp4_awsize,
  input bit [1 : 0] saxigp4_awburst,
  input bit_as_bool saxigp4_awlock,
  input bit [3 : 0] saxigp4_awcache,
  input bit [2 : 0] saxigp4_awprot,
  input bit_as_bool saxigp4_awvalid,
  output bit_as_bool saxigp4_awready,
  input bit [127 : 0] saxigp4_wdata,
  input bit [15 : 0] saxigp4_wstrb,
  input bit_as_bool saxigp4_wlast,
  input bit_as_bool saxigp4_wvalid,
  output bit_as_bool saxigp4_wready,
  output bit [5 : 0] saxigp4_bid,
  output bit [1 : 0] saxigp4_bresp,
  output bit_as_bool saxigp4_bvalid,
  input bit_as_bool saxigp4_bready,
  input bit [5 : 0] saxigp4_arid,
  input bit [48 : 0] saxigp4_araddr,
  input bit [7 : 0] saxigp4_arlen,
  input bit [2 : 0] saxigp4_arsize,
  input bit [1 : 0] saxigp4_arburst,
  input bit_as_bool saxigp4_arlock,
  input bit [3 : 0] saxigp4_arcache,
  input bit [2 : 0] saxigp4_arprot,
  input bit_as_bool saxigp4_arvalid,
  output bit_as_bool saxigp4_arready,
  output bit [5 : 0] saxigp4_rid,
  output bit [127 : 0] saxigp4_rdata,
  output bit [1 : 0] saxigp4_rresp,
  output bit_as_bool saxigp4_rlast,
  output bit_as_bool saxigp4_rvalid,
  input bit_as_bool saxigp4_rready,
  input bit [3 : 0] saxigp4_awqos,
  input bit [3 : 0] saxigp4_arqos,
  input bit_as_bool saxihp3_fpd_aclk,
  input bit_as_bool saxigp5_aruser,
  input bit_as_bool saxigp5_awuser,
  input bit [5 : 0] saxigp5_awid,
  input bit [48 : 0] saxigp5_awaddr,
  input bit [7 : 0] saxigp5_awlen,
  input bit [2 : 0] saxigp5_awsize,
  input bit [1 : 0] saxigp5_awburst,
  input bit_as_bool saxigp5_awlock,
  input bit [3 : 0] saxigp5_awcache,
  input bit [2 : 0] saxigp5_awprot,
  input bit_as_bool saxigp5_awvalid,
  output bit_as_bool saxigp5_awready,
  input bit [127 : 0] saxigp5_wdata,
  input bit [15 : 0] saxigp5_wstrb,
  input bit_as_bool saxigp5_wlast,
  input bit_as_bool saxigp5_wvalid,
  output bit_as_bool saxigp5_wready,
  output bit [5 : 0] saxigp5_bid,
  output bit [1 : 0] saxigp5_bresp,
  output bit_as_bool saxigp5_bvalid,
  input bit_as_bool saxigp5_bready,
  input bit [5 : 0] saxigp5_arid,
  input bit [48 : 0] saxigp5_araddr,
  input bit [7 : 0] saxigp5_arlen,
  input bit [2 : 0] saxigp5_arsize,
  input bit [1 : 0] saxigp5_arburst,
  input bit_as_bool saxigp5_arlock,
  input bit [3 : 0] saxigp5_arcache,
  input bit [2 : 0] saxigp5_arprot,
  input bit_as_bool saxigp5_arvalid,
  output bit_as_bool saxigp5_arready,
  output bit [5 : 0] saxigp5_rid,
  output bit [127 : 0] saxigp5_rdata,
  output bit [1 : 0] saxigp5_rresp,
  output bit_as_bool saxigp5_rlast,
  output bit_as_bool saxigp5_rvalid,
  input bit_as_bool saxigp5_rready,
  input bit [3 : 0] saxigp5_awqos,
  input bit [3 : 0] saxigp5_arqos,
  input bit [94 : 0] emio_gpio_i,
  output bit [94 : 0] emio_gpio_o,
  output bit [94 : 0] emio_gpio_t,
  input bit [2 : 0] pl_ps_irq0,
  output bit_as_bool pl_resetn0,
  output bit_as_bool pl_clk0
);
endmodule
`endif
