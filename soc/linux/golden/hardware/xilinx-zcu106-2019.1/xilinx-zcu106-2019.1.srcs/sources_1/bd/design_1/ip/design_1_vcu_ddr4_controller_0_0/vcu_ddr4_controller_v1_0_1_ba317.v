`timescale 1 ps / 1 ps
////-----------------------------------------------------------------------------
// ba317_ddr4_controller_v1_0
// 
//-----------------------------------------------------------------------------
//
// ************************************************************************
// ** DISCLAIMER OF LIABILITY                                            **
// **                                                                    **
// ** This file contains proprietary and confidential information of     **
// ** Xilinx, Inc. ("Xilinx"), that is distributed under a license       **
// ** from Xilinx, and may be used, copied and/or diSCLosed only         **
// ** pursuant to the terms of a valid license agreement with Xilinx.    **
// **                                                                    **
// ** XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION              **
// ** ("MATERIALS") "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER         **
// ** EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING WITHOUT                **
// ** LIMITATION, ANY WARRANTY WITH RESPECT TO NONINFRINGEMENT,          **
// ** MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE. Xilinx      **
// ** does not warrant that functions included in the Materials will     **
// ** meet the requirements of Licensee, or that the operation of the    **
// ** Materials will be uninterrupted or error-free, or that defects     **
// ** in the Materials will be corrected. Furthermore, Xilinx does       **
// ** not warrant or make any representations regarding use, or the      **
// ** results of the use, of the Materials in terms of correctness,      **
// ** accuracy, reliability or otherwise.                                **
// **                                                                    **
// ** Xilinx products are not designed or intended to be fail-safe,      **
// ** or for use in any application requiring fail-safe performance,     **
// ** such as life-support or safety devices or systems, Class III       **
// ** medical devices, nuclear facilities, applications related to       **
// ** the deployment of airbags, or any other applications that could    **
// ** lead to death, personal injury or severe property or               **
// ** environmental damage (individually and collectively, "critical     **
// ** applications"). Customer assumes the sole risk and liability       **
// ** of any use of Xilinx products in critical applications,            **
// ** subject only to applicable laws and regulations governing          **
// ** limitations on product liability.                                  **
// **                                                                    **
// ** Copyright 2016 Xilinx, Inc.                                        **
// ** All rights reserved.                                               **
// **                                                                    **
// ** This disclaimer and copyright notice must be retained as part      **
// ** of this file at all times.                                         **
// ************************************************************************
//
//-----------------------------------------------------------------------------
// Filename:    ba317_ddr4_controller_v1_0_ba317_ddr4_controller
// Version:       v1.00.a
// Description:   This is the wrapper file for ba317_ddr4_controller. 
//-----------------------------------------------------------------------------
// Structure:   This section shows the hierarchical structure of 
//              ba317_ddr4_controller_wrapper.
//
//              --ba317_ddr4_controller_v1_0_ba317_ddr4_controller.v
//                    --ba317_ddr4_controller.v - Unisim component
//-----------------------------------------------------------------------------
// Author:     Saif 
//
// History:
//
//prathib      03/30/2016      -- First version
// ~~~~~~
// Created the First version v1_0_v1.00.a
// ^^^^^^
//------------------------------------------------------------------------------


module vcu_ddr4_controller_v1_0_1_ba317
#(
parameter DQ_DATA_WIDTH      = 64,
parameter PORT0_DATA_WIDTH      = 128,
parameter PORT1_DATA_WIDTH      = 128,
parameter PORT2_DATA_WIDTH      = 128,
parameter PORT3_DATA_WIDTH      = 128,
parameter PORT4_DATA_WIDTH      = 128,

parameter DIMM_VALUE_HDL = 1,
parameter DRAM_WIDTH_HDL = 1,
parameter SPEED_BIN_HDL  = 0,
// XPE Variables
parameter ENABLE_PORT0 = "TRUE",
parameter ENABLE_PORT1 = "TRUE",
parameter ENABLE_PORT2 = "FALSE",
parameter ENABLE_PORT3 = "FALSE",
parameter ENABLE_PORT4 = "TRUE",
parameter HDL_PORT0_EN = "TRUE",
parameter HDL_PORT1_EN = "TRUE",
parameter HDL_PORT2_EN = "TRUE",
parameter HDL_PORT3_EN = "TRUE",
parameter HDL_PORT4_EN = "TRUE"

)
(


input wire  [31:0]                             pl_barco_slot_araddr0,
input wire  [1:0]                              pl_barco_slot_arburst0, 
input wire  [15:0]                             pl_barco_slot_arid0,    
input wire  [7:0]                              pl_barco_slot_arlen0,
input wire  [2:0]                              pl_barco_slot_arsize0,
input wire                                     pl_barco_slot_arvalid0,
output  wire                                   barco_pl_slot_arready0,


input wire  [15:0]                             pl_barco_slot_awid0,    
input wire  [31:0]                             pl_barco_slot_awaddr0,
input wire  [7:0]                              pl_barco_slot_awlen0,
input wire  [2:0]                              pl_barco_slot_awsize0,
input wire  [1:0]                              pl_barco_slot_awburst0,
input wire                                     pl_barco_slot_awvalid0,
output wire                                    barco_pl_slot_awready0,

output  wire  [15:0]                           barco_pl_slot_bid0,     
output  wire  [1:0]                            barco_pl_slot_bresp0,
output  wire                                   barco_pl_slot_bvalid0,
input wire                                     pl_barco_slot_bready0,

output  wire  [15:0]                           barco_pl_slot_rid0,     
output  wire  [PORT0_DATA_WIDTH-1:0]           barco_pl_slot_rdata0,
output  wire  [1:0]                            barco_pl_slot_rresp0,
output  wire                                   barco_pl_slot_rlast0,
output  wire                                   barco_pl_slot_rvalid0,
input wire                                     pl_barco_slot_rready0,


input wire  [15:0]                            pl_barco_slot_wid0,
input wire  [PORT0_DATA_WIDTH-1:0]            pl_barco_slot_wdata0,
input wire  [15:0]                            pl_barco_slot_wstrb0,
input wire                                    pl_barco_slot_wlast0,
input wire                                    pl_barco_slot_wvalid0,
output  wire                                  barco_pl_slot_wready0,


input wire  [31:0]                   pl_barco_slot_araddr1,
input wire  [1:0]                    pl_barco_slot_arburst1,
input wire  [15:0]                   pl_barco_slot_arid1,
input wire  [7:0]                    pl_barco_slot_arlen1,
input wire  [2:0]                    pl_barco_slot_arsize1,
input wire                           pl_barco_slot_arvalid1,
output wire                          barco_pl_slot_arready1,

input wire  [15:0]                   pl_barco_slot_awid1,
input wire  [31:0]                   pl_barco_slot_awaddr1,
input wire  [7:0]                    pl_barco_slot_awlen1,
input wire  [1:0]                    pl_barco_slot_awburst1,
input wire  [2:0]                    pl_barco_slot_awsize1,
input wire                           pl_barco_slot_awvalid1,
output  wire                         barco_pl_slot_awready1,



output  wire  [15:0]                 barco_pl_slot_bid1,
output  wire  [1:0]                  barco_pl_slot_bresp1,
output  wire                         barco_pl_slot_bvalid1,
input wire                           pl_barco_slot_bready1,


output  wire  [15:0]                 barco_pl_slot_rid1,
output  wire  [PORT0_DATA_WIDTH-1:0] barco_pl_slot_rdata1,
output  wire                         barco_pl_slot_rlast1,
output  wire                         barco_pl_slot_rvalid1,
output  wire  [1:0]                  barco_pl_slot_rresp1,
input wire                           pl_barco_slot_rready1,


input wire  [15:0]                   pl_barco_slot_wid1,
input wire  [PORT0_DATA_WIDTH-1:0]   pl_barco_slot_wdata1,
input wire  [15:0]                   pl_barco_slot_wstrb1,
input wire                           pl_barco_slot_wlast1,
input wire                           pl_barco_slot_wvalid1,
output  wire                         barco_pl_slot_wready1,



input wire  [31:0]                  pl_barco_slot_araddr4,
input wire  [1:0]                   pl_barco_slot_arburst4,
input wire  [15:0]                  pl_barco_slot_arid4,
input wire  [7:0]                   pl_barco_slot_arlen4,
input wire  [2:0]                   pl_barco_slot_arsize4,
input wire                          pl_barco_slot_arvalid4,
output  wire                        barco_pl_slot_arready4,


input wire  [31:0]                  pl_barco_slot_awaddr4,
input wire  [1:0]                   pl_barco_slot_awburst4,
input wire  [15:0]                  pl_barco_slot_awid4,
input wire  [7:0]                   pl_barco_slot_awlen4,
input wire  [2:0]                   pl_barco_slot_awsize4,
input wire                          pl_barco_slot_awvalid4,
output  wire                        barco_pl_slot_awready4,


input wire                          pl_barco_slot_bready4,
output  wire                        barco_pl_slot_bvalid4,
output  wire  [15:0]                barco_pl_slot_bid4,
output  wire  [1:0]                 barco_pl_slot_bresp4,


output  wire  [PORT0_DATA_WIDTH-1:0]barco_pl_slot_rdata4,
output  wire  [15:0]                barco_pl_slot_rid4,
output  wire                        barco_pl_slot_rlast4,
output  wire                        barco_pl_slot_rvalid4,
output  wire  [1:0]                 barco_pl_slot_rresp4,
input wire                          pl_barco_slot_rready4,

input wire  [15:0]                  pl_barco_slot_wid4,
input wire  [PORT0_DATA_WIDTH-1:0]  pl_barco_slot_wdata4,
input wire                          pl_barco_slot_wlast4,
input wire                          pl_barco_slot_wvalid4,
input wire  [15:0]                  pl_barco_slot_wstrb4,
output wire                         barco_pl_slot_wready4,


input wire  [31:0]                  pl_barco_slot_araddr3,
input wire  [1:0]                   pl_barco_slot_arburst3,
input wire  [15:0]                  pl_barco_slot_arid3,
input wire  [7:0]                   pl_barco_slot_arlen3,
input wire  [2:0]                   pl_barco_slot_arsize3,
input wire                          pl_barco_slot_arvalid3,
output  wire                        barco_pl_slot_arready3,

input wire  [31:0]                  pl_barco_slot_awaddr3,
input wire  [1:0]                   pl_barco_slot_awburst3,
input wire  [15:0]                  pl_barco_slot_awid3,
input wire  [7:0]                   pl_barco_slot_awlen3,
input wire  [2:0]                   pl_barco_slot_awsize3,
input wire                          pl_barco_slot_awvalid3,
output  wire                        barco_pl_slot_awready3,

input wire                          pl_barco_slot_bready3,
output  wire                        barco_pl_slot_bvalid3,
output  wire  [15:0]                barco_pl_slot_bid3,
output  wire  [1:0]                 barco_pl_slot_bresp3,


output wire  [PORT0_DATA_WIDTH-1:0] barco_pl_slot_rdata3,
output wire  [15:0]                 barco_pl_slot_rid3,
output wire                         barco_pl_slot_rlast3,
output wire                         barco_pl_slot_rvalid3,
output wire  [1:0]                  barco_pl_slot_rresp3,
input wire                          pl_barco_slot_rready3,

input wire  [15:0]                  pl_barco_slot_wid3,
input wire  [PORT0_DATA_WIDTH-1:0]  pl_barco_slot_wdata3,
input wire                          pl_barco_slot_wlast3,
input wire                          pl_barco_slot_wvalid3,
input wire  [15:0]                  pl_barco_slot_wstrb3,
output  wire                        barco_pl_slot_wready3,


input wire  [31:0]                  pl_barco_slot_araddr2,
input wire  [1:0]                   pl_barco_slot_arburst2,
input wire  [15:0]                  pl_barco_slot_arid2,
input wire  [7:0]                   pl_barco_slot_arlen2,
input wire  [2:0]                   pl_barco_slot_arsize2,
input wire                          pl_barco_slot_arvalid2,
output  wire                        barco_pl_slot_arready2,

input wire  [31:0]                  pl_barco_slot_awaddr2,
input wire  [1:0]                   pl_barco_slot_awburst2,
input wire  [15:0]                  pl_barco_slot_awid2,
input wire  [7:0]                   pl_barco_slot_awlen2,
input wire  [2:0]                   pl_barco_slot_awsize2,
input wire                          pl_barco_slot_awvalid2,
output  wire                        barco_pl_slot_awready2,

input wire                          pl_barco_slot_bready2,
output  wire                        barco_pl_slot_bvalid2,
output  wire  [15:0]                barco_pl_slot_bid2,
output  wire  [1:0]                 barco_pl_slot_bresp2,


output  wire  [PORT0_DATA_WIDTH-1:0] barco_pl_slot_rdata2,
output  wire  [15:0]                 barco_pl_slot_rid2,
output  wire                         barco_pl_slot_rlast2,
output  wire                         barco_pl_slot_rvalid2,
output  wire  [1:0]                  barco_pl_slot_rresp2,
input wire                           pl_barco_slot_rready2,


input wire  [15:0]                   pl_barco_slot_wid2,
input wire  [PORT0_DATA_WIDTH-1:0]   pl_barco_slot_wdata2,
input wire                           pl_barco_slot_wlast2,
input wire                           pl_barco_slot_wvalid2,
input wire  [15:0]                   pl_barco_slot_wstrb2,
output  wire                         barco_pl_slot_wready2,




input wire                       S_Axi_Clk,
input wire                       S_Axi_Rst, 
input wire                       sys_rst, 
output wire                      InitDone        ,
output reg                       sRst_Out        ,
output wire                      UsrClk          ,
output wire                      Fail_wrDataEn   , 
output wire  [1:0]               c0_ddr4_ba      ,
inout wire   [7:0]               c0_ddr4_dqs_c   ,
inout wire   [7:0]               c0_ddr4_dqs_t   ,
output wire                      c0_ddr4_ck_c    ,
output  wire                     c0_ddr4_ck_t    ,
output wire  [16:0]              c0_ddr4_adr     , 
output wire                      c0_ddr4_cke     ,
output wire                      c0_ddr4_odt     ,
output wire                      c0_ddr4_reset_n ,
output wire  [DQ_DATA_WIDTH-1:0] c0_ddr4_dq      ,
output wire                      c0_ddr4_act_n   ,
output wire [0:0]                c0_ddr4_bg,
output wire  [7:0]               c0_ddr4_dm_dbi_n,
output wire                      c0_ddr4_cs_n ,
input wire                       c0_sys_clk_p ,
input  wire                      c0_sys_clk_n 

);
         
 wire                      sRst_o   ;
 reg                       q1,q2;

       
//          DDR4Ctrl_0  for x8 and DDR4Ctrl_1 for x16   
           DDR4Ctrl_1 i_DDR4Ctrl (
		      .Wr0sRst    (S_Axi_Rst),
              .Wr0Clk     (S_Axi_Clk ),
              .Wr0AWID    (pl_barco_slot_awid0),  
              .Wr0AWADDR  (pl_barco_slot_awaddr0),  
              .Wr0AWLEN   (pl_barco_slot_awlen0), 
              .Wr0AWSIZE  (pl_barco_slot_awsize0),
              .Wr0AWBURST (pl_barco_slot_awburst0),
              .Wr0AWVALID (pl_barco_slot_awvalid0),
              .Wr0AWREADY (barco_pl_slot_awready0),
              .Wr0WID     (pl_barco_slot_wid0),    
              .Wr0WDATA   (pl_barco_slot_wdata0), 
              .Wr0WSTRB   (pl_barco_slot_wstrb0), 
              .Wr0WLAST   (pl_barco_slot_wlast0), 
              .Wr0WVALID  (pl_barco_slot_wvalid0), 
              .Wr0WREADY  (barco_pl_slot_wready0), 
			  .Wr0BID     (barco_pl_slot_bid0),   
              .Wr0BRESP   (barco_pl_slot_bresp0), 
              .Wr0BVALID  (barco_pl_slot_bvalid0),  
              .Wr0BREADY  (pl_barco_slot_bready0),
              .Wr1sRst     (S_Axi_Rst),                      
              .Wr1Clk      (S_Axi_Clk ),                       
              .Wr1AWID   (pl_barco_slot_awid1),    
              .Wr1AWADDR (pl_barco_slot_awaddr1),                        
              .Wr1AWLEN  (pl_barco_slot_awlen1),   
              .Wr1AWSIZE (pl_barco_slot_awsize1),  
              .Wr1AWBURST(pl_barco_slot_awburst1)  ,                      
              .Wr1AWVALID(pl_barco_slot_awvalid1)   ,                     
              .Wr1AWREADY(barco_pl_slot_awready1) ,                       
              .Wr1WID     (pl_barco_slot_wid1),    
              .Wr1WDATA  (pl_barco_slot_wdata1),                         
              .Wr1WSTRB  (pl_barco_slot_wstrb1),                       
              .Wr1WLAST  (pl_barco_slot_wlast1), 
              .Wr1WVALID (pl_barco_slot_wvalid1),
              .Wr1WREADY (barco_pl_slot_wready1),
              .Wr1BID    (barco_pl_slot_bid1),    
              .Wr1BRESP  (barco_pl_slot_bresp1), 
              .Wr1BVALID (barco_pl_slot_bvalid1),
              .Wr1BREADY (pl_barco_slot_bready1),
              .Rd0sRst   (S_Axi_Rst),  
              .Rd0Clk    (S_Axi_Clk ),  
              .Rd0ARID   (pl_barco_slot_arid0), 
              .Rd0ARADDR (pl_barco_slot_araddr0),
              .Rd0ARLEN  (pl_barco_slot_arlen0),  
              .Rd0ARSIZE (pl_barco_slot_arsize0),
              .Rd0ARBURST(pl_barco_slot_arburst0), 
              .Rd0ARVALID(pl_barco_slot_arvalid0) ,
              .Rd0ARREADY(barco_pl_slot_arready0),
              .Rd0RID    (barco_pl_slot_rid0),
              .Rd0RDATA  (barco_pl_slot_rdata0),
              .Rd0RRESP  (barco_pl_slot_rresp0),
              .Rd0RLAST  (barco_pl_slot_rlast0),
              .Rd0RVALID (barco_pl_slot_rvalid0),
              .Rd0RREADY (pl_barco_slot_rready0),    
              .Rd1sRst     (S_Axi_Rst),                       
              .Rd1Clk      (S_Axi_Clk ), 
              .Rd1ARID    (pl_barco_slot_arid1),    
              .Rd1ARADDR  (pl_barco_slot_araddr1),  
              .Rd1ARLEN   (pl_barco_slot_arlen1),   
              .Rd1ARSIZE  (pl_barco_slot_arsize1),  
              .Rd1ARBURST (pl_barco_slot_arburst1) ,
              .Rd1ARVALID (pl_barco_slot_arvalid1) ,
              .Rd1ARREADY (barco_pl_slot_arready1),
              .Rd1RID     (barco_pl_slot_rid1),
              .Rd1RDATA   (barco_pl_slot_rdata1),
              .Rd1RRESP   (barco_pl_slot_rresp1),
              .Rd1RLAST   (barco_pl_slot_rlast1),
              .Rd1RVALID  (barco_pl_slot_rvalid1),
              .Rd1RREADY  (pl_barco_slot_rready1), 
              .Wr2sRst     (S_Axi_Rst),  
              .Wr2Clk      (S_Axi_Clk ),  
              .Wr2AWID    (pl_barco_slot_awid2),    
              .Wr2AWADDR (pl_barco_slot_awaddr2),  
              .Wr2AWLEN  (pl_barco_slot_awlen2),   
              .Wr2AWSIZE (pl_barco_slot_awsize2),  
              .Wr2AWBURST(pl_barco_slot_awburst2) , 
              .Wr2AWVALID(pl_barco_slot_awvalid2) , 
              .Wr2AWREADY(barco_pl_slot_awready2) ,                       
              .Wr2WID    (pl_barco_slot_wid2),                           
              .Wr2WDATA  (pl_barco_slot_wdata2),   
              .Wr2WSTRB  (pl_barco_slot_wstrb2),   
              .Wr2WLAST  (pl_barco_slot_wlast2),                         
              .Wr2WVALID (pl_barco_slot_wvalid2),  
              .Wr2WREADY (barco_pl_slot_wready2),  
              .Wr2BID    (barco_pl_slot_bid2),      
              .Wr2BRESP  (barco_pl_slot_bresp2),                          
              .Wr2BVALID (barco_pl_slot_bvalid2),  
              .Wr2BREADY (pl_barco_slot_bready2),  
              .Wr3sRst     (S_Axi_Rst),  
              .Wr3Clk      (S_Axi_Clk ),  
              .Wr3AWID    (pl_barco_slot_awid3),      
              .Wr3AWADDR  (pl_barco_slot_awaddr3),    
              .Wr3AWLEN  (pl_barco_slot_awlen3),     
              .Wr3AWSIZE (pl_barco_slot_awsize3),    
              .Wr3AWBURST(pl_barco_slot_awburst3)  ,  
              .Wr3AWVALID(pl_barco_slot_awvalid3)   , 
              .Wr3AWREADY(barco_pl_slot_awready3)   , 
              .Wr3WID    (pl_barco_slot_wid3),       
              .Wr3WDATA  (pl_barco_slot_wdata3),     
              .Wr3WSTRB  (pl_barco_slot_wstrb3),     
              .Wr3WLAST  (pl_barco_slot_wlast3),     
              .Wr3WVALID (pl_barco_slot_wvalid3),    
              .Wr3WREADY (barco_pl_slot_wready3),    
              .Wr3BID    (barco_pl_slot_bid3),       
              .Wr3BRESP  (barco_pl_slot_bresp3),     
              .Wr3BVALID (barco_pl_slot_bvalid3),    
              .Wr3BREADY (pl_barco_slot_bready3),    
              .Wr4sRst    (S_Axi_Rst),  
              .Wr4Clk     (S_Axi_Clk ),  
              .Wr4AWID    (pl_barco_slot_awid4),      
              .Wr4AWADDR  (pl_barco_slot_awaddr4),    
              .Wr4AWLEN   (pl_barco_slot_awlen4),     
              .Wr4AWSIZE  (pl_barco_slot_awsize4),    
              .Wr4AWBURST (pl_barco_slot_awburst4) ,   
              .Wr4AWVALID (pl_barco_slot_awvalid4)  ,  
              .Wr4AWREADY (barco_pl_slot_awready4)   , 
              .Wr4WID     (pl_barco_slot_wid4),       
              .Wr4WDATA   (pl_barco_slot_wdata4),     
              .Wr4WSTRB   (pl_barco_slot_wstrb4),     
              .Wr4WLAST   (pl_barco_slot_wlast4),     
              .Wr4WVALID  (pl_barco_slot_wvalid4),    
              .Wr4WREADY  (barco_pl_slot_wready4),    
              .Wr4BID     (barco_pl_slot_bid4),       
              .Wr4BRESP   (barco_pl_slot_bresp4),     
              .Wr4BVALID  (barco_pl_slot_bvalid4),    
              .Wr4BREADY  (pl_barco_slot_bready4),    
              .Rd2sRst    (S_Axi_Rst),  
              .Rd2Clk     (S_Axi_Clk ),  
              .Rd2ARID   (pl_barco_slot_arid2),    
              .Rd2ARADDR (pl_barco_slot_araddr2),  
              .Rd2ARLEN  (pl_barco_slot_arlen2),   
              .Rd2ARSIZE (pl_barco_slot_arsize2),  
              .Rd2ARBURST(pl_barco_slot_arburst2)  ,
              .Rd2ARVALID(pl_barco_slot_arvalid2)  ,
              .Rd2ARREADY(barco_pl_slot_arready2)  ,
              .Rd2RID    (barco_pl_slot_rid2),     
              .Rd2RDATA  (barco_pl_slot_rdata2),   
              .Rd2RRESP  (barco_pl_slot_rresp2),   
              .Rd2RLAST  (barco_pl_slot_rlast2),   
              .Rd2RVALID (barco_pl_slot_rvalid2),  
              .Rd2RREADY (pl_barco_slot_rready2),  
              .Rd3sRst    (S_Axi_Rst),  
              .Rd3Clk     (S_Axi_Clk ),  
              .Rd3ARID    (pl_barco_slot_arid3),    
              .Rd3ARADDR (pl_barco_slot_araddr3),  
              .Rd3ARLEN  (pl_barco_slot_arlen3),   
              .Rd3ARSIZE (pl_barco_slot_arsize3),  
              .Rd3ARBURST(pl_barco_slot_arburst3),  
              .Rd3ARVALID(pl_barco_slot_arvalid3),  
              .Rd3ARREADY(barco_pl_slot_arready3),  
              .Rd3RID    (barco_pl_slot_rid3),     
              .Rd3RDATA  (barco_pl_slot_rdata3),   
              .Rd3RRESP  (barco_pl_slot_rresp3),   
              .Rd3RLAST  (barco_pl_slot_rlast3),   
              .Rd3RVALID (barco_pl_slot_rvalid3),  
              .Rd3RREADY (pl_barco_slot_rready3),  
              .Rd4sRst          (S_Axi_Rst),  
              .Rd4Clk           (S_Axi_Clk ),  
              .Rd4ARID          (pl_barco_slot_arid4),    
              .Rd4ARADDR        (pl_barco_slot_araddr4),  
              .Rd4ARLEN         (pl_barco_slot_arlen4),   
              .Rd4ARSIZE        (pl_barco_slot_arsize4),  
              .Rd4ARBURST       (pl_barco_slot_arburst4) , 
              .Rd4ARVALID       (pl_barco_slot_arvalid4) , 
              .Rd4ARREADY       (barco_pl_slot_arready4) , 
              .Rd4RID           (barco_pl_slot_rid4),     
              .Rd4RDATA         (barco_pl_slot_rdata4),   
              .Rd4RRESP         (barco_pl_slot_rresp4),   
              .Rd4RLAST         (barco_pl_slot_rlast4),   
              .Rd4RVALID        (barco_pl_slot_rvalid4),  
              .Rd4RREADY        (pl_barco_slot_rready4),
              .InitDone         (InitDone     ),
              .sRst_o           (sRst_o          ),  
              .sys_rst          (sys_rst         ),  
              .UsrClk           (UsrClk          ),  
              .Fail_wrDataEn    (Fail_wrDataEn   ),  
              .c0_ddr4_ba       (c0_ddr4_ba      ),  
              .c0_ddr4_dqs_c    (c0_ddr4_dqs_c   ),  
              .c0_ddr4_dqs_t    (c0_ddr4_dqs_t   ),  
              .c0_ddr4_ck_c     (c0_ddr4_ck_c    ),  
              .c0_ddr4_ck_t     (c0_ddr4_ck_t    ),  
              .c0_ddr4_adr      (c0_ddr4_adr     ),  
              .c0_ddr4_cke      (c0_ddr4_cke     ),  
              .c0_ddr4_odt      (c0_ddr4_odt     ),  
              .c0_ddr4_reset_n  (c0_ddr4_reset_n ),  
              .c0_ddr4_dq       (c0_ddr4_dq      ),  
              .c0_ddr4_act_n    (c0_ddr4_act_n   ),  
              .c0_ddr4_bg       (c0_ddr4_bg      ),  
              .c0_ddr4_dm_dbi_n (c0_ddr4_dm_dbi_n),  
              .c0_ddr4_cs_n     (c0_ddr4_cs_n    ),  
              .c0_sys_clk_p     ( c0_sys_clk_p   ) , 
              .c0_sys_clk_n     ( c0_sys_clk_n  )        
			  );
 


always@(posedge UsrClk)
begin
        q1<= sRst_o  ;
        q2<= q1  ;
        sRst_Out <= q2 ;
end

endmodule
