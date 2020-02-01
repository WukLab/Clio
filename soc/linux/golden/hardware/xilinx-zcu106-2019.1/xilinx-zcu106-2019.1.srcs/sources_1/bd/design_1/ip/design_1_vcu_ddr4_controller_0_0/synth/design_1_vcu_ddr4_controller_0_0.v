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


// IP VLNV: xilinx.com:ip:vcu_ddr4_controller:1.0
// IP Revision: 1

(* X_CORE_INFO = "vcu_ddr4_controller_v1_0_1_ba317,Vivado 2019.1" *)
(* CHECK_LICENSE_TYPE = "design_1_vcu_ddr4_controller_0_0,vcu_ddr4_controller_v1_0_1_ba317,{}" *)
(* CORE_GENERATION_INFO = "design_1_vcu_ddr4_controller_0_0,vcu_ddr4_controller_v1_0_1_ba317,{x_ipProduct=Vivado 2019.1,x_ipVendor=xilinx.com,x_ipLibrary=ip,x_ipName=vcu_ddr4_controller,x_ipVersion=1.0,x_ipCoreRevision=1,x_ipLanguage=VERILOG,x_ipSimLanguage=MIXED,DIMM_VALUE_HDL=1,DRAM_WIDTH_HDL=1,SPEED_BIN_HDL=1,HDL_PORT0_EN=1,HDL_PORT1_EN=1,HDL_PORT2_EN=1,HDL_PORT3_EN=1,HDL_PORT4_EN=1}" *)
(* DowngradeIPIdentifiedWarnings = "yes" *)
module design_1_vcu_ddr4_controller_0_0 (
  S_Axi_Clk,
  S_Axi_Rst,
  pl_barco_slot_wstrb0,
  pl_barco_slot_wstrb1,
  pl_barco_slot_wstrb2,
  pl_barco_slot_wstrb3,
  pl_barco_slot_wstrb4,
  pl_barco_slot_araddr0,
  pl_barco_slot_arburst0,
  pl_barco_slot_arid0,
  pl_barco_slot_arlen0,
  barco_pl_slot_arready0,
  pl_barco_slot_arsize0,
  pl_barco_slot_arvalid0,
  pl_barco_slot_awaddr0,
  pl_barco_slot_awburst0,
  pl_barco_slot_awid0,
  pl_barco_slot_awlen0,
  barco_pl_slot_awready0,
  pl_barco_slot_awsize0,
  pl_barco_slot_awvalid0,
  pl_barco_slot_bready0,
  barco_pl_slot_bvalid0,
  barco_pl_slot_bid0,
  barco_pl_slot_rdata0,
  barco_pl_slot_rid0,
  barco_pl_slot_rlast0,
  pl_barco_slot_rready0,
  barco_pl_slot_rvalid0,
  pl_barco_slot_wid0,
  pl_barco_slot_wdata0,
  pl_barco_slot_wlast0,
  barco_pl_slot_bresp0,
  barco_pl_slot_rresp0,
  barco_pl_slot_wready0,
  pl_barco_slot_wvalid0,
  pl_barco_slot_araddr1,
  pl_barco_slot_arburst1,
  pl_barco_slot_arid1,
  pl_barco_slot_arlen1,
  barco_pl_slot_arready1,
  pl_barco_slot_arsize1,
  pl_barco_slot_arvalid1,
  pl_barco_slot_awaddr1,
  pl_barco_slot_awburst1,
  pl_barco_slot_awid1,
  pl_barco_slot_awlen1,
  barco_pl_slot_awready1,
  pl_barco_slot_awsize1,
  pl_barco_slot_awvalid1,
  pl_barco_slot_bready1,
  barco_pl_slot_bvalid1,
  barco_pl_slot_bid1,
  barco_pl_slot_rdata1,
  barco_pl_slot_rid1,
  barco_pl_slot_rlast1,
  pl_barco_slot_rready1,
  barco_pl_slot_rvalid1,
  pl_barco_slot_wid1,
  pl_barco_slot_wdata1,
  pl_barco_slot_wlast1,
  barco_pl_slot_bresp1,
  barco_pl_slot_rresp1,
  barco_pl_slot_wready1,
  pl_barco_slot_wvalid1,
  pl_barco_slot_araddr2,
  pl_barco_slot_arburst2,
  pl_barco_slot_arid2,
  pl_barco_slot_arlen2,
  barco_pl_slot_arready2,
  pl_barco_slot_arsize2,
  pl_barco_slot_arvalid2,
  pl_barco_slot_awaddr2,
  pl_barco_slot_awburst2,
  pl_barco_slot_awid2,
  pl_barco_slot_awlen2,
  barco_pl_slot_awready2,
  pl_barco_slot_awsize2,
  pl_barco_slot_awvalid2,
  pl_barco_slot_bready2,
  barco_pl_slot_bvalid2,
  barco_pl_slot_bid2,
  barco_pl_slot_rdata2,
  barco_pl_slot_rid2,
  barco_pl_slot_rlast2,
  pl_barco_slot_rready2,
  barco_pl_slot_rvalid2,
  pl_barco_slot_wid2,
  pl_barco_slot_wdata2,
  pl_barco_slot_wlast2,
  barco_pl_slot_bresp2,
  barco_pl_slot_rresp2,
  barco_pl_slot_wready2,
  pl_barco_slot_wvalid2,
  pl_barco_slot_araddr3,
  pl_barco_slot_arburst3,
  pl_barco_slot_arid3,
  pl_barco_slot_arlen3,
  barco_pl_slot_arready3,
  pl_barco_slot_arsize3,
  pl_barco_slot_arvalid3,
  pl_barco_slot_awaddr3,
  pl_barco_slot_awburst3,
  pl_barco_slot_awid3,
  pl_barco_slot_awlen3,
  barco_pl_slot_awready3,
  pl_barco_slot_awsize3,
  pl_barco_slot_awvalid3,
  pl_barco_slot_bready3,
  barco_pl_slot_bvalid3,
  barco_pl_slot_bid3,
  barco_pl_slot_rdata3,
  barco_pl_slot_rid3,
  barco_pl_slot_rlast3,
  pl_barco_slot_rready3,
  barco_pl_slot_rvalid3,
  pl_barco_slot_wid3,
  pl_barco_slot_wdata3,
  pl_barco_slot_wlast3,
  barco_pl_slot_bresp3,
  barco_pl_slot_rresp3,
  barco_pl_slot_wready3,
  pl_barco_slot_wvalid3,
  pl_barco_slot_araddr4,
  pl_barco_slot_arburst4,
  pl_barco_slot_arid4,
  pl_barco_slot_arlen4,
  barco_pl_slot_arready4,
  pl_barco_slot_arsize4,
  pl_barco_slot_arvalid4,
  pl_barco_slot_awaddr4,
  pl_barco_slot_awburst4,
  pl_barco_slot_awid4,
  pl_barco_slot_awlen4,
  barco_pl_slot_awready4,
  pl_barco_slot_awsize4,
  pl_barco_slot_awvalid4,
  pl_barco_slot_bready4,
  barco_pl_slot_bvalid4,
  barco_pl_slot_bid4,
  barco_pl_slot_rdata4,
  barco_pl_slot_rid4,
  barco_pl_slot_rlast4,
  pl_barco_slot_rready4,
  barco_pl_slot_rvalid4,
  pl_barco_slot_wid4,
  pl_barco_slot_wdata4,
  pl_barco_slot_wlast4,
  barco_pl_slot_bresp4,
  barco_pl_slot_rresp4,
  barco_pl_slot_wready4,
  pl_barco_slot_wvalid4,
  c0_ddr4_act_n,
  c0_ddr4_adr,
  c0_ddr4_ba,
  c0_ddr4_bg,
  c0_ddr4_cke,
  c0_ddr4_ck_t,
  c0_ddr4_ck_c,
  c0_ddr4_cs_n,
  c0_ddr4_dm_dbi_n,
  c0_ddr4_dq,
  c0_ddr4_dqs_c,
  c0_ddr4_dqs_t,
  c0_ddr4_odt,
  c0_ddr4_reset_n,
  c0_sys_clk_p,
  c0_sys_clk_n,
  UsrClk,
  sRst_Out,
  sys_rst,
  InitDone
);

(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_CLK, ASSOCIATED_BUSIF S_AXI_PORT0:S_AXI_PORT1:S_AXI_PORT2:S_AXI_PORT3:S_AXI_PORT4, FREQ_HZ 250000000, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 S_AXI_CLK CLK" *)
input wire S_Axi_Clk;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME SAXI_RST, POLARITY ACTIVE_HIGH, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 SAXI_RST RstIn" *)
input wire S_Axi_Rst;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 WSTRB" *)
input wire [15 : 0] pl_barco_slot_wstrb0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 WSTRB" *)
input wire [15 : 0] pl_barco_slot_wstrb1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 WSTRB" *)
input wire [15 : 0] pl_barco_slot_wstrb2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 WSTRB" *)
input wire [15 : 0] pl_barco_slot_wstrb3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 WSTRB" *)
input wire [15 : 0] pl_barco_slot_wstrb4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARADDR" *)
input wire [31 : 0] pl_barco_slot_araddr0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARBURST" *)
input wire [1 : 0] pl_barco_slot_arburst0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARID" *)
input wire [15 : 0] pl_barco_slot_arid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARLEN" *)
input wire [7 : 0] pl_barco_slot_arlen0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARREADY" *)
output wire barco_pl_slot_arready0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARSIZE" *)
input wire [2 : 0] pl_barco_slot_arsize0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 ARVALID" *)
input wire pl_barco_slot_arvalid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWADDR" *)
input wire [31 : 0] pl_barco_slot_awaddr0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWBURST" *)
input wire [1 : 0] pl_barco_slot_awburst0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWID" *)
input wire [15 : 0] pl_barco_slot_awid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWLEN" *)
input wire [7 : 0] pl_barco_slot_awlen0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWREADY" *)
output wire barco_pl_slot_awready0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWSIZE" *)
input wire [2 : 0] pl_barco_slot_awsize0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 AWVALID" *)
input wire pl_barco_slot_awvalid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 BREADY" *)
input wire pl_barco_slot_bready0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 BVALID" *)
output wire barco_pl_slot_bvalid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 BID" *)
output wire [15 : 0] barco_pl_slot_bid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 RDATA" *)
output wire [127 : 0] barco_pl_slot_rdata0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 RID" *)
output wire [15 : 0] barco_pl_slot_rid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 RLAST" *)
output wire barco_pl_slot_rlast0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 RREADY" *)
input wire pl_barco_slot_rready0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 RVALID" *)
output wire barco_pl_slot_rvalid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 WID" *)
input wire [15 : 0] pl_barco_slot_wid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 WDATA" *)
input wire [127 : 0] pl_barco_slot_wdata0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 WLAST" *)
input wire pl_barco_slot_wlast0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 BRESP" *)
output wire [1 : 0] barco_pl_slot_bresp0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 RRESP" *)
output wire [1 : 0] barco_pl_slot_rresp0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 WREADY" *)
output wire barco_pl_slot_wready0;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_PORT0, DATA_WIDTH 128, PROTOCOL AXI4, FREQ_HZ 250000000, ID_WIDTH 16, ADDR_WIDTH 32, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 0, HAS_PROT 0, HAS_CACHE 0, HAS_QOS 0, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 0, NUM_READ_OUTSTANDING 32, NUM_WRITE_OUTSTANDING 32, MAX_BURST_LENGTH 256, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, NUM_READ_\
THREADS 1, NUM_WRITE_THREADS 1, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT0 WVALID" *)
input wire pl_barco_slot_wvalid0;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARADDR" *)
input wire [31 : 0] pl_barco_slot_araddr1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARBURST" *)
input wire [1 : 0] pl_barco_slot_arburst1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARID" *)
input wire [15 : 0] pl_barco_slot_arid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARLEN" *)
input wire [7 : 0] pl_barco_slot_arlen1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARREADY" *)
output wire barco_pl_slot_arready1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARSIZE" *)
input wire [2 : 0] pl_barco_slot_arsize1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 ARVALID" *)
input wire pl_barco_slot_arvalid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWADDR" *)
input wire [31 : 0] pl_barco_slot_awaddr1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWBURST" *)
input wire [1 : 0] pl_barco_slot_awburst1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWID" *)
input wire [15 : 0] pl_barco_slot_awid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWLEN" *)
input wire [7 : 0] pl_barco_slot_awlen1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWREADY" *)
output wire barco_pl_slot_awready1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWSIZE" *)
input wire [2 : 0] pl_barco_slot_awsize1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 AWVALID" *)
input wire pl_barco_slot_awvalid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 BREADY" *)
input wire pl_barco_slot_bready1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 BVALID" *)
output wire barco_pl_slot_bvalid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 BID" *)
output wire [15 : 0] barco_pl_slot_bid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 RDATA" *)
output wire [127 : 0] barco_pl_slot_rdata1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 RID" *)
output wire [15 : 0] barco_pl_slot_rid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 RLAST" *)
output wire barco_pl_slot_rlast1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 RREADY" *)
input wire pl_barco_slot_rready1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 RVALID" *)
output wire barco_pl_slot_rvalid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 WID" *)
input wire [15 : 0] pl_barco_slot_wid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 WDATA" *)
input wire [127 : 0] pl_barco_slot_wdata1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 WLAST" *)
input wire pl_barco_slot_wlast1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 BRESP" *)
output wire [1 : 0] barco_pl_slot_bresp1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 RRESP" *)
output wire [1 : 0] barco_pl_slot_rresp1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 WREADY" *)
output wire barco_pl_slot_wready1;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_PORT1, DATA_WIDTH 128, PROTOCOL AXI4, FREQ_HZ 250000000, ID_WIDTH 16, ADDR_WIDTH 32, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 0, HAS_PROT 0, HAS_CACHE 0, HAS_QOS 0, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 0, NUM_READ_OUTSTANDING 32, NUM_WRITE_OUTSTANDING 32, MAX_BURST_LENGTH 256, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, NUM_READ_\
THREADS 1, NUM_WRITE_THREADS 1, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT1 WVALID" *)
input wire pl_barco_slot_wvalid1;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARADDR" *)
input wire [31 : 0] pl_barco_slot_araddr2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARBURST" *)
input wire [1 : 0] pl_barco_slot_arburst2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARID" *)
input wire [15 : 0] pl_barco_slot_arid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARLEN" *)
input wire [7 : 0] pl_barco_slot_arlen2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARREADY" *)
output wire barco_pl_slot_arready2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARSIZE" *)
input wire [2 : 0] pl_barco_slot_arsize2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 ARVALID" *)
input wire pl_barco_slot_arvalid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWADDR" *)
input wire [31 : 0] pl_barco_slot_awaddr2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWBURST" *)
input wire [1 : 0] pl_barco_slot_awburst2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWID" *)
input wire [15 : 0] pl_barco_slot_awid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWLEN" *)
input wire [7 : 0] pl_barco_slot_awlen2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWREADY" *)
output wire barco_pl_slot_awready2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWSIZE" *)
input wire [2 : 0] pl_barco_slot_awsize2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 AWVALID" *)
input wire pl_barco_slot_awvalid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 BREADY" *)
input wire pl_barco_slot_bready2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 BVALID" *)
output wire barco_pl_slot_bvalid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 BID" *)
output wire [15 : 0] barco_pl_slot_bid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 RDATA" *)
output wire [127 : 0] barco_pl_slot_rdata2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 RID" *)
output wire [15 : 0] barco_pl_slot_rid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 RLAST" *)
output wire barco_pl_slot_rlast2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 RREADY" *)
input wire pl_barco_slot_rready2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 RVALID" *)
output wire barco_pl_slot_rvalid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 WID" *)
input wire [15 : 0] pl_barco_slot_wid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 WDATA" *)
input wire [127 : 0] pl_barco_slot_wdata2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 WLAST" *)
input wire pl_barco_slot_wlast2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 BRESP" *)
output wire [1 : 0] barco_pl_slot_bresp2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 RRESP" *)
output wire [1 : 0] barco_pl_slot_rresp2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 WREADY" *)
output wire barco_pl_slot_wready2;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_PORT2, DATA_WIDTH 128, PROTOCOL AXI4, FREQ_HZ 250000000, ID_WIDTH 16, ADDR_WIDTH 32, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 0, HAS_PROT 0, HAS_CACHE 0, HAS_QOS 0, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 1, NUM_READ_OUTSTANDING 32, NUM_WRITE_OUTSTANDING 32, MAX_BURST_LENGTH 256, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, NUM_READ_\
THREADS 1, NUM_WRITE_THREADS 1, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT2 WVALID" *)
input wire pl_barco_slot_wvalid2;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARADDR" *)
input wire [31 : 0] pl_barco_slot_araddr3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARBURST" *)
input wire [1 : 0] pl_barco_slot_arburst3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARID" *)
input wire [15 : 0] pl_barco_slot_arid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARLEN" *)
input wire [7 : 0] pl_barco_slot_arlen3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARREADY" *)
output wire barco_pl_slot_arready3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARSIZE" *)
input wire [2 : 0] pl_barco_slot_arsize3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 ARVALID" *)
input wire pl_barco_slot_arvalid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWADDR" *)
input wire [31 : 0] pl_barco_slot_awaddr3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWBURST" *)
input wire [1 : 0] pl_barco_slot_awburst3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWID" *)
input wire [15 : 0] pl_barco_slot_awid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWLEN" *)
input wire [7 : 0] pl_barco_slot_awlen3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWREADY" *)
output wire barco_pl_slot_awready3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWSIZE" *)
input wire [2 : 0] pl_barco_slot_awsize3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 AWVALID" *)
input wire pl_barco_slot_awvalid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 BREADY" *)
input wire pl_barco_slot_bready3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 BVALID" *)
output wire barco_pl_slot_bvalid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 BID" *)
output wire [15 : 0] barco_pl_slot_bid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 RDATA" *)
output wire [127 : 0] barco_pl_slot_rdata3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 RID" *)
output wire [15 : 0] barco_pl_slot_rid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 RLAST" *)
output wire barco_pl_slot_rlast3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 RREADY" *)
input wire pl_barco_slot_rready3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 RVALID" *)
output wire barco_pl_slot_rvalid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 WID" *)
input wire [15 : 0] pl_barco_slot_wid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 WDATA" *)
input wire [127 : 0] pl_barco_slot_wdata3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 WLAST" *)
input wire pl_barco_slot_wlast3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 BRESP" *)
output wire [1 : 0] barco_pl_slot_bresp3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 RRESP" *)
output wire [1 : 0] barco_pl_slot_rresp3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 WREADY" *)
output wire barco_pl_slot_wready3;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_PORT3, DATA_WIDTH 128, PROTOCOL AXI4, FREQ_HZ 250000000, ID_WIDTH 16, ADDR_WIDTH 32, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 0, HAS_PROT 0, HAS_CACHE 0, HAS_QOS 0, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 1, NUM_READ_OUTSTANDING 8, NUM_WRITE_OUTSTANDING 8, MAX_BURST_LENGTH 256, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, NUM_READ_TH\
READS 4, NUM_WRITE_THREADS 4, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT3 WVALID" *)
input wire pl_barco_slot_wvalid3;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARADDR" *)
input wire [31 : 0] pl_barco_slot_araddr4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARBURST" *)
input wire [1 : 0] pl_barco_slot_arburst4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARID" *)
input wire [15 : 0] pl_barco_slot_arid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARLEN" *)
input wire [7 : 0] pl_barco_slot_arlen4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARREADY" *)
output wire barco_pl_slot_arready4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARSIZE" *)
input wire [2 : 0] pl_barco_slot_arsize4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 ARVALID" *)
input wire pl_barco_slot_arvalid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWADDR" *)
input wire [31 : 0] pl_barco_slot_awaddr4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWBURST" *)
input wire [1 : 0] pl_barco_slot_awburst4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWID" *)
input wire [15 : 0] pl_barco_slot_awid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWLEN" *)
input wire [7 : 0] pl_barco_slot_awlen4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWREADY" *)
output wire barco_pl_slot_awready4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWSIZE" *)
input wire [2 : 0] pl_barco_slot_awsize4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 AWVALID" *)
input wire pl_barco_slot_awvalid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 BREADY" *)
input wire pl_barco_slot_bready4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 BVALID" *)
output wire barco_pl_slot_bvalid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 BID" *)
output wire [15 : 0] barco_pl_slot_bid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 RDATA" *)
output wire [127 : 0] barco_pl_slot_rdata4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 RID" *)
output wire [15 : 0] barco_pl_slot_rid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 RLAST" *)
output wire barco_pl_slot_rlast4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 RREADY" *)
input wire pl_barco_slot_rready4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 RVALID" *)
output wire barco_pl_slot_rvalid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 WID" *)
input wire [15 : 0] pl_barco_slot_wid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 WDATA" *)
input wire [127 : 0] pl_barco_slot_wdata4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 WLAST" *)
input wire pl_barco_slot_wlast4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 BRESP" *)
output wire [1 : 0] barco_pl_slot_bresp4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 RRESP" *)
output wire [1 : 0] barco_pl_slot_rresp4;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 WREADY" *)
output wire barco_pl_slot_wready4;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_PORT4, DATA_WIDTH 128, PROTOCOL AXI4, FREQ_HZ 250000000, ID_WIDTH 16, ADDR_WIDTH 32, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 0, HAS_PROT 0, HAS_CACHE 0, HAS_QOS 0, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 0, NUM_READ_OUTSTANDING 4, NUM_WRITE_OUTSTANDING 2, MAX_BURST_LENGTH 16, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, NUM_READ_THR\
EADS 1, NUM_WRITE_THREADS 1, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_PORT4 WVALID" *)
input wire pl_barco_slot_wvalid4;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 ACT_N" *)
output wire [0 : 0] c0_ddr4_act_n;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 ADR" *)
output wire [16 : 0] c0_ddr4_adr;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 BA" *)
output wire [1 : 0] c0_ddr4_ba;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 BG" *)
output wire [0 : 0] c0_ddr4_bg;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 CKE" *)
output wire [0 : 0] c0_ddr4_cke;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 CK_T" *)
output wire [0 : 0] c0_ddr4_ck_t;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 CK_C" *)
output wire [0 : 0] c0_ddr4_ck_c;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 CS_N" *)
output wire [0 : 0] c0_ddr4_cs_n;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 DM_N" *)
inout wire [7 : 0] c0_ddr4_dm_dbi_n;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 DQ" *)
inout wire [63 : 0] c0_ddr4_dq;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 DQS_C" *)
inout wire [7 : 0] c0_ddr4_dqs_c;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 DQS_T" *)
inout wire [7 : 0] c0_ddr4_dqs_t;
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 ODT" *)
output wire [0 : 0] c0_ddr4_odt;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME C0_DDR4, CAN_DEBUG false, TIMEPERIOD_PS 1250, MEMORY_TYPE COMPONENTS, DATA_WIDTH 8, CS_ENABLED true, DATA_MASK_ENABLED true, SLOT Single, MEM_ADDR_MAP ROW_COLUMN_BANK, BURST_LENGTH 8, AXI_ARBITRATION_SCHEME TDM, CAS_LATENCY 11, CAS_WRITE_LATENCY 11" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:ddr4:1.0 C0_DDR4 RESET_N" *)
output wire [0 : 0] c0_ddr4_reset_n;
(* X_INTERFACE_INFO = "xilinx.com:interface:diff_clock:1.0 c0_sys CLK_P" *)
input wire [0 : 0] c0_sys_clk_p;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME c0_sys, BOARD.ASSOCIATED_PARAM C0_CLOCK_BOARD_INTERFACE, CAN_DEBUG false, FREQ_HZ 125000000" *)
(* X_INTERFACE_INFO = "xilinx.com:interface:diff_clock:1.0 c0_sys CLK_N" *)
input wire [0 : 0] c0_sys_clk_n;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME USER_CLOCK, FREQ_HZ 250000000, PHASE 0.000, CLK_DOMAIN design_1_vcu_ddr4_controller_0_0_UsrClk, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 USER_CLOCK UserClock" *)
output wire UsrClk;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME SRST_O, POLARITY ACTIVE_HIGH, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 SRST_O SRST_O" *)
output wire sRst_Out;
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME sys_rst, POLARITY ACTIVE_HIGH, INSERT_VIP 0" *)
(* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 sys_rst RstIn" *)
input wire sys_rst;
output wire InitDone;

  vcu_ddr4_controller_v1_0_1_ba317 #(
    .DIMM_VALUE_HDL(1),
    .DRAM_WIDTH_HDL(1),
    .SPEED_BIN_HDL(1),
    .HDL_PORT0_EN(1),
    .HDL_PORT1_EN(1),
    .HDL_PORT2_EN(1),
    .HDL_PORT3_EN(1),
    .HDL_PORT4_EN(1)
  ) inst (
    .S_Axi_Clk(S_Axi_Clk),
    .S_Axi_Rst(S_Axi_Rst),
    .pl_barco_slot_wstrb0(pl_barco_slot_wstrb0),
    .pl_barco_slot_wstrb1(pl_barco_slot_wstrb1),
    .pl_barco_slot_wstrb2(pl_barco_slot_wstrb2),
    .pl_barco_slot_wstrb3(pl_barco_slot_wstrb3),
    .pl_barco_slot_wstrb4(pl_barco_slot_wstrb4),
    .pl_barco_slot_araddr0(pl_barco_slot_araddr0),
    .pl_barco_slot_arburst0(pl_barco_slot_arburst0),
    .pl_barco_slot_arid0(pl_barco_slot_arid0),
    .pl_barco_slot_arlen0(pl_barco_slot_arlen0),
    .barco_pl_slot_arready0(barco_pl_slot_arready0),
    .pl_barco_slot_arsize0(pl_barco_slot_arsize0),
    .pl_barco_slot_arvalid0(pl_barco_slot_arvalid0),
    .pl_barco_slot_awaddr0(pl_barco_slot_awaddr0),
    .pl_barco_slot_awburst0(pl_barco_slot_awburst0),
    .pl_barco_slot_awid0(pl_barco_slot_awid0),
    .pl_barco_slot_awlen0(pl_barco_slot_awlen0),
    .barco_pl_slot_awready0(barco_pl_slot_awready0),
    .pl_barco_slot_awsize0(pl_barco_slot_awsize0),
    .pl_barco_slot_awvalid0(pl_barco_slot_awvalid0),
    .pl_barco_slot_bready0(pl_barco_slot_bready0),
    .barco_pl_slot_bvalid0(barco_pl_slot_bvalid0),
    .barco_pl_slot_bid0(barco_pl_slot_bid0),
    .barco_pl_slot_rdata0(barco_pl_slot_rdata0),
    .barco_pl_slot_rid0(barco_pl_slot_rid0),
    .barco_pl_slot_rlast0(barco_pl_slot_rlast0),
    .pl_barco_slot_rready0(pl_barco_slot_rready0),
    .barco_pl_slot_rvalid0(barco_pl_slot_rvalid0),
    .pl_barco_slot_wid0(pl_barco_slot_wid0),
    .pl_barco_slot_wdata0(pl_barco_slot_wdata0),
    .pl_barco_slot_wlast0(pl_barco_slot_wlast0),
    .barco_pl_slot_bresp0(barco_pl_slot_bresp0),
    .barco_pl_slot_rresp0(barco_pl_slot_rresp0),
    .barco_pl_slot_wready0(barco_pl_slot_wready0),
    .pl_barco_slot_wvalid0(pl_barco_slot_wvalid0),
    .pl_barco_slot_araddr1(pl_barco_slot_araddr1),
    .pl_barco_slot_arburst1(pl_barco_slot_arburst1),
    .pl_barco_slot_arid1(pl_barco_slot_arid1),
    .pl_barco_slot_arlen1(pl_barco_slot_arlen1),
    .barco_pl_slot_arready1(barco_pl_slot_arready1),
    .pl_barco_slot_arsize1(pl_barco_slot_arsize1),
    .pl_barco_slot_arvalid1(pl_barco_slot_arvalid1),
    .pl_barco_slot_awaddr1(pl_barco_slot_awaddr1),
    .pl_barco_slot_awburst1(pl_barco_slot_awburst1),
    .pl_barco_slot_awid1(pl_barco_slot_awid1),
    .pl_barco_slot_awlen1(pl_barco_slot_awlen1),
    .barco_pl_slot_awready1(barco_pl_slot_awready1),
    .pl_barco_slot_awsize1(pl_barco_slot_awsize1),
    .pl_barco_slot_awvalid1(pl_barco_slot_awvalid1),
    .pl_barco_slot_bready1(pl_barco_slot_bready1),
    .barco_pl_slot_bvalid1(barco_pl_slot_bvalid1),
    .barco_pl_slot_bid1(barco_pl_slot_bid1),
    .barco_pl_slot_rdata1(barco_pl_slot_rdata1),
    .barco_pl_slot_rid1(barco_pl_slot_rid1),
    .barco_pl_slot_rlast1(barco_pl_slot_rlast1),
    .pl_barco_slot_rready1(pl_barco_slot_rready1),
    .barco_pl_slot_rvalid1(barco_pl_slot_rvalid1),
    .pl_barco_slot_wid1(pl_barco_slot_wid1),
    .pl_barco_slot_wdata1(pl_barco_slot_wdata1),
    .pl_barco_slot_wlast1(pl_barco_slot_wlast1),
    .barco_pl_slot_bresp1(barco_pl_slot_bresp1),
    .barco_pl_slot_rresp1(barco_pl_slot_rresp1),
    .barco_pl_slot_wready1(barco_pl_slot_wready1),
    .pl_barco_slot_wvalid1(pl_barco_slot_wvalid1),
    .pl_barco_slot_araddr2(pl_barco_slot_araddr2),
    .pl_barco_slot_arburst2(pl_barco_slot_arburst2),
    .pl_barco_slot_arid2(pl_barco_slot_arid2),
    .pl_barco_slot_arlen2(pl_barco_slot_arlen2),
    .barco_pl_slot_arready2(barco_pl_slot_arready2),
    .pl_barco_slot_arsize2(pl_barco_slot_arsize2),
    .pl_barco_slot_arvalid2(pl_barco_slot_arvalid2),
    .pl_barco_slot_awaddr2(pl_barco_slot_awaddr2),
    .pl_barco_slot_awburst2(pl_barco_slot_awburst2),
    .pl_barco_slot_awid2(pl_barco_slot_awid2),
    .pl_barco_slot_awlen2(pl_barco_slot_awlen2),
    .barco_pl_slot_awready2(barco_pl_slot_awready2),
    .pl_barco_slot_awsize2(pl_barco_slot_awsize2),
    .pl_barco_slot_awvalid2(pl_barco_slot_awvalid2),
    .pl_barco_slot_bready2(pl_barco_slot_bready2),
    .barco_pl_slot_bvalid2(barco_pl_slot_bvalid2),
    .barco_pl_slot_bid2(barco_pl_slot_bid2),
    .barco_pl_slot_rdata2(barco_pl_slot_rdata2),
    .barco_pl_slot_rid2(barco_pl_slot_rid2),
    .barco_pl_slot_rlast2(barco_pl_slot_rlast2),
    .pl_barco_slot_rready2(pl_barco_slot_rready2),
    .barco_pl_slot_rvalid2(barco_pl_slot_rvalid2),
    .pl_barco_slot_wid2(pl_barco_slot_wid2),
    .pl_barco_slot_wdata2(pl_barco_slot_wdata2),
    .pl_barco_slot_wlast2(pl_barco_slot_wlast2),
    .barco_pl_slot_bresp2(barco_pl_slot_bresp2),
    .barco_pl_slot_rresp2(barco_pl_slot_rresp2),
    .barco_pl_slot_wready2(barco_pl_slot_wready2),
    .pl_barco_slot_wvalid2(pl_barco_slot_wvalid2),
    .pl_barco_slot_araddr3(pl_barco_slot_araddr3),
    .pl_barco_slot_arburst3(pl_barco_slot_arburst3),
    .pl_barco_slot_arid3(pl_barco_slot_arid3),
    .pl_barco_slot_arlen3(pl_barco_slot_arlen3),
    .barco_pl_slot_arready3(barco_pl_slot_arready3),
    .pl_barco_slot_arsize3(pl_barco_slot_arsize3),
    .pl_barco_slot_arvalid3(pl_barco_slot_arvalid3),
    .pl_barco_slot_awaddr3(pl_barco_slot_awaddr3),
    .pl_barco_slot_awburst3(pl_barco_slot_awburst3),
    .pl_barco_slot_awid3(pl_barco_slot_awid3),
    .pl_barco_slot_awlen3(pl_barco_slot_awlen3),
    .barco_pl_slot_awready3(barco_pl_slot_awready3),
    .pl_barco_slot_awsize3(pl_barco_slot_awsize3),
    .pl_barco_slot_awvalid3(pl_barco_slot_awvalid3),
    .pl_barco_slot_bready3(pl_barco_slot_bready3),
    .barco_pl_slot_bvalid3(barco_pl_slot_bvalid3),
    .barco_pl_slot_bid3(barco_pl_slot_bid3),
    .barco_pl_slot_rdata3(barco_pl_slot_rdata3),
    .barco_pl_slot_rid3(barco_pl_slot_rid3),
    .barco_pl_slot_rlast3(barco_pl_slot_rlast3),
    .pl_barco_slot_rready3(pl_barco_slot_rready3),
    .barco_pl_slot_rvalid3(barco_pl_slot_rvalid3),
    .pl_barco_slot_wid3(pl_barco_slot_wid3),
    .pl_barco_slot_wdata3(pl_barco_slot_wdata3),
    .pl_barco_slot_wlast3(pl_barco_slot_wlast3),
    .barco_pl_slot_bresp3(barco_pl_slot_bresp3),
    .barco_pl_slot_rresp3(barco_pl_slot_rresp3),
    .barco_pl_slot_wready3(barco_pl_slot_wready3),
    .pl_barco_slot_wvalid3(pl_barco_slot_wvalid3),
    .pl_barco_slot_araddr4(pl_barco_slot_araddr4),
    .pl_barco_slot_arburst4(pl_barco_slot_arburst4),
    .pl_barco_slot_arid4(pl_barco_slot_arid4),
    .pl_barco_slot_arlen4(pl_barco_slot_arlen4),
    .barco_pl_slot_arready4(barco_pl_slot_arready4),
    .pl_barco_slot_arsize4(pl_barco_slot_arsize4),
    .pl_barco_slot_arvalid4(pl_barco_slot_arvalid4),
    .pl_barco_slot_awaddr4(pl_barco_slot_awaddr4),
    .pl_barco_slot_awburst4(pl_barco_slot_awburst4),
    .pl_barco_slot_awid4(pl_barco_slot_awid4),
    .pl_barco_slot_awlen4(pl_barco_slot_awlen4),
    .barco_pl_slot_awready4(barco_pl_slot_awready4),
    .pl_barco_slot_awsize4(pl_barco_slot_awsize4),
    .pl_barco_slot_awvalid4(pl_barco_slot_awvalid4),
    .pl_barco_slot_bready4(pl_barco_slot_bready4),
    .barco_pl_slot_bvalid4(barco_pl_slot_bvalid4),
    .barco_pl_slot_bid4(barco_pl_slot_bid4),
    .barco_pl_slot_rdata4(barco_pl_slot_rdata4),
    .barco_pl_slot_rid4(barco_pl_slot_rid4),
    .barco_pl_slot_rlast4(barco_pl_slot_rlast4),
    .pl_barco_slot_rready4(pl_barco_slot_rready4),
    .barco_pl_slot_rvalid4(barco_pl_slot_rvalid4),
    .pl_barco_slot_wid4(pl_barco_slot_wid4),
    .pl_barco_slot_wdata4(pl_barco_slot_wdata4),
    .pl_barco_slot_wlast4(pl_barco_slot_wlast4),
    .barco_pl_slot_bresp4(barco_pl_slot_bresp4),
    .barco_pl_slot_rresp4(barco_pl_slot_rresp4),
    .barco_pl_slot_wready4(barco_pl_slot_wready4),
    .pl_barco_slot_wvalid4(pl_barco_slot_wvalid4),
    .c0_ddr4_act_n(c0_ddr4_act_n),
    .c0_ddr4_adr(c0_ddr4_adr),
    .c0_ddr4_ba(c0_ddr4_ba),
    .c0_ddr4_bg(c0_ddr4_bg),
    .c0_ddr4_cke(c0_ddr4_cke),
    .c0_ddr4_ck_t(c0_ddr4_ck_t),
    .c0_ddr4_ck_c(c0_ddr4_ck_c),
    .c0_ddr4_cs_n(c0_ddr4_cs_n),
    .c0_ddr4_dm_dbi_n(c0_ddr4_dm_dbi_n),
    .c0_ddr4_dq(c0_ddr4_dq),
    .c0_ddr4_dqs_c(c0_ddr4_dqs_c),
    .c0_ddr4_dqs_t(c0_ddr4_dqs_t),
    .c0_ddr4_odt(c0_ddr4_odt),
    .c0_ddr4_reset_n(c0_ddr4_reset_n),
    .c0_sys_clk_p(c0_sys_clk_p),
    .c0_sys_clk_n(c0_sys_clk_n),
    .UsrClk(UsrClk),
    .sRst_Out(sRst_Out),
    .sys_rst(sys_rst),
    .InitDone(InitDone)
  );
endmodule
