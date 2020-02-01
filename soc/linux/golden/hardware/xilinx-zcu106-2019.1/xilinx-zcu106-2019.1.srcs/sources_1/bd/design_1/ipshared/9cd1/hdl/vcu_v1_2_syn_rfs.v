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
// Filename:    vcu_v1_2_1_registers
// Version:       v1.00.a
// Description:   This is the softIP for registers
//-----------------------------------------------------------------------------
// Structure:   This section shows the hierarchical structure of 
//              vcu_wrapper.
//
//              --vcu_v1_2_1_vcu.v
//                    --VCU.v - Unisim component
//                    --vcu_v1_2_1_registers.v
//-----------------------------------------------------------------------------
// Author:      kvikrama
//
// History:
//
//------------------------------------------------------------------------------


module vcu_v1_2_1_registers
#(
parameter C_VCU_AXILITEAPB_DATA_WIDTH = 32,
parameter HDL_TEST_PORT_EN = 0,
parameter HDL_ENCODER_EN = 1,
parameter HDL_DECODER_EN = 1,
parameter HDL_TABLE_NO = 2,
parameter HDL_MEM_DEPTH = 2048, // 2K
parameter HDL_RAM_TYPE = 1,
parameter HDL_PLL_BYPASS = 1,
parameter HDL_ENC_CLK = 200,
parameter HDL_MEMORY_SIZE = 2, // 2K
parameter HDL_COLOR_DEPTH = 0,
parameter HDL_FRAME_SIZE_X = 0,
parameter HDL_FRAME_SIZE_Y = 0,
parameter HDL_COLOR_FORMAT = 0,
parameter HDL_FPS = 0,
parameter HDL_VIDEO_STANDARD = 0,
parameter HDL_CODING_TYPE = 0,
parameter HDL_ENC_BUFFER_EN = 0,
parameter HDL_MCU_CLK = 0,
parameter HDL_PLL_CLK_HI = 34,
parameter HDL_PLL_CLK_LO = 27,
parameter HDL_CORE_CLK = 0,
parameter HDL_NUM_STREAMS = 1,
parameter HDL_MAX_NUM_CORES = 1,
parameter HDL_NUM_CONCURRENT_STREAMS = 1,

parameter HDL_DEC_VIDEO_STANDARD = 0,
parameter HDL_DEC_FRAME_SIZE_X = 0,
parameter HDL_DEC_FRAME_SIZE_Y = 0,
parameter HDL_DEC_FPS = 0,

parameter HDL_ENC_BUFFER_MANUAL_OVERRIDE = 0,
parameter HDL_ENC_BUFFER_MOTION_VEC_RANGE = 0,
parameter HDL_ENC_BUFFER_B_FRAME = 0,
parameter HDL_WPP_EN = 0,
parameter HDL_VCU_TEST_EN = 0, // new Variable

parameter HDL_AXI_ENC_CLK = 99,
parameter HDL_AXI_DEC_CLK = 99,
parameter HDL_AXI_MCU_CLK = 99,
parameter HDL_AXI_ENC_BASE0 = 99,
parameter HDL_AXI_ENC_BASE1 = 99,
parameter HDL_AXI_DEC_BASE0 = 99,
parameter HDL_AXI_DEC_BASE1 = 99,
parameter HDL_AXI_MCU_BASE = 99,
parameter HDL_AXI_ENC_RANGE0 = 99,
parameter HDL_AXI_ENC_RANGE1 = 99,
parameter HDL_AXI_DEC_RANGE0 = 99,
parameter HDL_AXI_DEC_RANGE1 = 99,
parameter HDL_AXI_MCU_RANGE = 99 

)
(

//VCU-AXI-SLAVE-External-Bus-In
input  wire                                     pl_vcu_axi_lite_clk,
input  wire                                     pl_vcu_raw_rst_n,

input  wire                                     core_clk,
input  wire                                     enc_buffer_clk,
input  wire                                     m_axi_mcu_aclk,
output wire                                     vcu_resetn_soft,
output wire                                     vcu_resetn_soft_ec,

//Read Address
input  wire  [19:0]                             pl_vcu_araddr_axi_lite_apb,
input  wire  [2:0]                              pl_vcu_arprot_axi_lite_apb,
input  wire                                     pl_vcu_arvalid_axi_lite_apb,
input  wire                                     vcu_pl_arready_axi_lite_apb, // this is now input from hard-macro

//Write Address
input  wire  [19:0]                             pl_vcu_awaddr_axi_lite_apb,
input  wire  [2:0]                              pl_vcu_awprot_axi_lite_apb,
input  wire                                     pl_vcu_awvalid_axi_lite_apb,
input  wire                                     vcu_pl_awready_axi_lite_apb, // this is now input from hard-macro

//Write Address to HM
output  wire  [19:0]                            lc_vcu_awaddr_axi_lite_apb,
output  wire  [2:0]                             lc_vcu_awprot_axi_lite_apb,
//output  wire                                  lc_vcu_awvalid_axi_lite_apb,

//From VCU-Hardmacro for read data
input  wire  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0]  vcu_pl_rdata_axi_lite_apb,
input  wire  [1:0]                              vcu_pl_rresp_axi_lite_apb,
input  wire                                     vcu_pl_rvalid_axi_lite_apb,
input  wire                                     pl_vcu_rready_axi_lite_apb,

//Write data-bus
input  wire  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0]  pl_vcu_wdata_axi_lite_apb,
input  wire  [3:0]                              pl_vcu_wstrb_axi_lite_apb,
input  wire                                     pl_vcu_wvalid_axi_lite_apb,
input  wire                                     vcu_pl_wready_axi_lite_apb,


//Write data-bus - HM
output  wire  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0] lc_vcu_wdata_axi_lite_apb,
output  wire  [3:0]                             lc_vcu_wstrb_axi_lite_apb,
//output  wire                                  lc_vcu_wvalid_axi_lite_apb,

//write-Resp Channel
input wire  [1:0]                               vcu_pl_bresp_axi_lite_apb,
input wire                                      vcu_pl_bvalid_axi_lite_apb,
input wire                                      pl_vcu_bready_axi_lite_apb,

output [1:0]                                    lc_pl_bresp_axi_lite_apb,
output                                          lc_pl_bvalid_axi_lite_apb,
output  wire                                    lc_vcu_bready_axi_lite_apb,

//Status bits to be captured in registers
input  wire                                     vcu_pl_core_status_clk_pll,
input  wire                                     vcu_pl_mcu_status_clk_pll,
input  wire                                     vcu_pl_pll_status_pll_lock,
input  wire                                     vcu_pl_pwr_supply_status_vccaux,
input  wire                                     vcu_pl_pwr_supply_status_vcuint,
input  wire                                     vcu_pl_pintreq,
output                                          vcu_gasket_enable,

//Clock enable
output                                          clock_high_enable,
output                                          clock_low_enable,


//VCU-AXI-SLAVE-Internal-Bus-to-VCU
// Write-address port to VCU-hardmacro
output                                    lc_vcu_awvalid_axi_lite_apb,// this is the feedback for address accept to PL
output                                    lc_pl_awready_axi_lite_apb, // this is the feedback for address accept to PL 
// Write-data port to VCU-hardmacro
output                                    lc_vcu_wvalid_axi_lite_apb,
output                                    lc_pl_wready_axi_lite_apb,

// Read-Address port to VCU-Hardmacro
output                                    lc_vcu_arvalid_axi_lite_apb, // this is the address valid to hard-macro
output                                    lc_pl_arready_axi_lite_apb, // this is the feedback for address accept to PL

// Read-data port to PL
output  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0]  lc_pl_rdata_axi_lite_apb,
output  [1:0]                              lc_pl_rresp_axi_lite_apb,
output                                     lc_pl_rvalid_axi_lite_apb,
output                                     lc_vcu_rready_axi_lite_apb,

//MCU Address
input wire [43:0]                          vcu_pl_mcu_m_axi_ic_dc_awaddr, 
input wire                                 vcu_pl_mcu_m_axi_ic_dc_awvalid,
input wire                                 pl_vcu_mcu_m_axi_ic_dc_awready,

output	[3:0]                              vcu_pll_test_sel,
output	[2:0]                              vcu_pll_test_ck_sel,
output	                                   vcu_pll_test_fract_en,
output	                                   vcu_pll_test_fract_clk_sel

);


// Read variables
wire  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0] lc_pl_rdata_axi_lite_apb;
wire  [1:0]                             lc_pl_rresp_axi_lite_apb;
wire                                    lc_pl_rvalid_axi_lite_apb;
wire                                    lc_vcu_rready_axi_lite_apb;
wire                                    lc_vcu_arvalid_axi_lite_apb; // this is the new address valid to hard-macro
wire                                    lc_pl_arready_axi_lite_apb; // this is the new feedback for address accept to PL
wire                                    lc_vcu_arvalid_axi_lite_apb_hm; // read to hard-macro
wire                                    lc_vcu_arvalid_axi_lite_apb_encoder_en; //
wire                                    lc_vcu_arvalid_axi_lite_apb_decoder_en;
wire                                    lc_vcu_arvalid_axi_lite_apb_memory_depth;
wire                                    lc_vcu_arvalid_axi_lite_apb_color_depth;
wire                                    lc_vcu_arvalid_axi_lite_apb_vertical_range;
wire                                    lc_vcu_arvalid_axi_lite_apb_frame_size_x;
wire                                    lc_vcu_arvalid_axi_lite_apb_frame_size_y;
wire                                    lc_vcu_arvalid_axi_lite_apb_color_format;
wire                                    lc_vcu_arvalid_axi_lite_apb_fps;
wire                                    lc_vcu_arvalid_axi_lite_apb_mcu_clk;
wire                                    lc_vcu_arvalid_axi_lite_apb_core_clk;
wire                                    lc_vcu_arvalid_axi_lite_apb_pll_clk;
wire                                    lc_vcu_arvalid_axi_lite_apb_enc_clk;
wire                                    lc_vcu_arvalid_axi_lite_apb_pll_bypass;



reg  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0]  soft_ip_reg_capture;
reg  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0]  soft_ip_reg_capture_r = 32'b0;
reg  [1:0]                              sip_rresp_r = 2'b0;
reg                                     sip_rvalid_r = 1'b0;
wire [15:0]                             sip_valid; 
wire                                    soft_ip_reg_range;

wire [12:0]                             sip_addr_valid; 
wire                                    soft_ip_addr_reg_range;
reg  [C_VCU_AXILITEAPB_DATA_WIDTH-1:0]  soft_ip_addr_reg_capture;

wire                                    lc_vcu_arvalid_axi_enc_clk;
wire                                    lc_vcu_arvalid_axi_dec_clk;
wire                                    lc_vcu_arvalid_axi_mcu_clk;
wire                                    lc_vcu_arvalid_mcu_base_addr;
wire                                    lc_vcu_arvalid_mcu_range;
wire                                    lc_vcu_arvalid_enc0_base_addr;
wire                                    lc_vcu_arvalid_enc0_range;
wire                                    lc_vcu_arvalid_enc1_base_addr;
wire                                    lc_vcu_arvalid_enc1_range;
wire                                    lc_vcu_arvalid_dec0_base_addr;
wire                                    lc_vcu_arvalid_dec0_range;
wire                                    lc_vcu_arvalid_dec1_base_addr;
wire                                    lc_vcu_arvalid_dec1_range;


// Write-variables
reg [1:0]                               lc_bresp = 2'b0;
reg                                     lc_bvalid;
reg                                     hm_bvalid; // un-used .. remove
wire                                    external_write;
//reg                                   lc_wready;
wire                                    lc_awready;
reg                                     vcu_gasket_enable = 1'b1;
reg [1:0]                               wt_addr_lc_capture; // 00 -> no addr, 01 -> lc, 10-> hm, 11->s/m
reg [1:0]                               rd_addr_lc_capture; // 00 -> no addr, 01 -> lc, 10-> hm, 11->s/m
reg [2:0]                               decoded_reg; // 00 -> gasket&reset, 01 -> mcu_base0, 10-> mcu_base1, 11-> pll_setting
wire [43:0]                             mcu_wt_snoop_addr_pll_change_enc;
wire [43:0]                             mcu_wt_snoop_addr_pll_change_dec;
reg [31:0]                              mcu_wt_snoop_addr_pll_change_low_enc;
reg [11:0]                              mcu_wt_snoop_addr_pll_change_hi_enc;
reg [31:0]                              mcu_wt_snoop_addr_pll_change_low_dec;
reg [11:0]                              mcu_wt_snoop_addr_pll_change_hi_dec;
reg [15:0]                              pll_high_low_value;
reg                                     pll_div_change_done;
reg                                     pll_div_change_done_reg;

wire                                    trigger_pll_div_change;
wire  [31:0]                            pll_div_value;

wire                                    vcu_reset_gen;
reg                                     vcu_reset_capture = 1'b1;
(* ASYNC_REG = "TRUE" *) reg            vcu_reset_f1 = 1'b1;
(* ASYNC_REG = "TRUE" *) reg            vcu_reset_f2 = 1'b1;
(* ASYNC_REG = "TRUE" *) reg            vcu_reset_encClk_f1 = 1'b1;
(* ASYNC_REG = "TRUE" *) reg            vcu_reset_encClk_f2 = 1'b1;
(* ASYNC_REG = "TRUE" *) reg            vcu_reset_encClk_f3 = 1'b1;
reg                                     disable_mcp_patch = 1'b1;


wire [3:0]                               vcu_pll_test_sel;
wire [2:0]                               vcu_pll_test_ck_sel;
wire                                     vcu_pll_test_fract_en;
wire                                     vcu_pll_test_fract_clk_sel;

wire                                     clock_high_enable;
wire                                     clock_low_enable;

// Statemachine access
wire    bus_free ;
wire    bus_external_lock;
reg  [3:0] cState, nState;
reg  [19:0] sm_waddr;
reg  [31:0] sm_wdata;
wire        addr_w_done;
wire        data_w_done;
wire        data_w_resp;
wire [3:0]  num_concurrent_streams; 
wire [3:0]  num_streams; 

assign  vcu_pll_test_sel  = 4'b1111;
assign  vcu_pll_test_ck_sel  = 3'b000;
assign  vcu_pll_test_fract_en  = 1'b1;
assign  vcu_pll_test_fract_clk_sel  = 1'b1;
assign  num_concurrent_streams = HDL_NUM_CONCURRENT_STREAMS;
assign  num_streams = HDL_NUM_STREAMS;


assign mcu_wt_snoop_addr_pll_change_enc = {mcu_wt_snoop_addr_pll_change_hi_enc, mcu_wt_snoop_addr_pll_change_low_enc};
assign mcu_wt_snoop_addr_pll_change_dec = {mcu_wt_snoop_addr_pll_change_hi_dec, mcu_wt_snoop_addr_pll_change_low_dec};


//reset (soft)
assign vcu_resetn_soft = /*pl_vcu_raw_rst_n & */ vcu_reset_gen;


always @ (posedge pl_vcu_axi_lite_clk)
begin
//   if (pl_vcu_raw_rst_n == 1'b0) begin
//     vcu_reset_f1 <= 1'b1;
//     vcu_reset_f2 <= 1'b1;
//   end else  begin
     vcu_reset_f1 <= vcu_reset_capture;
     vcu_reset_f2 <= vcu_reset_f1;
//   end
end

assign  vcu_reset_gen = vcu_reset_f2;


always @ (posedge  pl_vcu_axi_lite_clk) begin
     vcu_reset_encClk_f1 <= vcu_resetn_soft;
end


always @ (posedge enc_buffer_clk)
begin
//   if (pl_vcu_raw_rst_n == 1'b0) begin
//     vcu_reset_encClk_f1 <= 1'b1;
//     vcu_reset_encClk_f2 <= 1'b1;
//   end else  begin
     vcu_reset_encClk_f2 <= vcu_reset_encClk_f1;
     vcu_reset_encClk_f3 <= vcu_reset_encClk_f2;
//   end
end

assign  vcu_resetn_soft_ec =  vcu_reset_encClk_f3 ;

////////////////////////////////////////////////////////////////
// Bug fix for the default read..
//assign lc_pl_arready_axi_lite_apb  = vcu_pl_arready_axi_lite_apb; // this control is tied 1'b1 always
// Need to gate the external bus lock
assign lc_pl_arready_axi_lite_apb  = (bus_external_lock == 1'b1) ? 1'b0 : vcu_pl_arready_axi_lite_apb | soft_ip_reg_range | soft_ip_addr_reg_range ;

////////////////////////////////////////////////////////////////
assign lc_vcu_arvalid_axi_lite_apb = (bus_external_lock == 1'b1) ? 1'b0 : lc_vcu_arvalid_axi_lite_apb_hm ;

//Write-path
//Write-address channel
assign lc_vcu_awvalid_axi_lite_apb = (wt_addr_lc_capture == 2'b10) ?  pl_vcu_awvalid_axi_lite_apb : 
                                     (wt_addr_lc_capture == 2'b11 && (cState == 'h1 | cState == 'h4)) ? 1'b1 : 1'b0;
assign lc_vcu_awaddr_axi_lite_apb  = (wt_addr_lc_capture == 2'b10) ?  pl_vcu_awaddr_axi_lite_apb  : 
                                     (wt_addr_lc_capture == 2'b11 && (cState == 'h1 | cState == 'h4)) ? sm_waddr : 'h0;  
assign lc_vcu_awprot_axi_lite_apb  = (wt_addr_lc_capture == 2'b10) ?  pl_vcu_awprot_axi_lite_apb  : 'h0;


// Need to gate the external bus lock
assign  lc_pl_awready_axi_lite_apb = (bus_external_lock == 1'b1) ? 1'b0 : 
                                     (wt_addr_lc_capture == 2'b01) ? lc_awready : 
									 (wt_addr_lc_capture == 2'b10) ? vcu_pl_awready_axi_lite_apb : 1'b0;


assign  lc_awready = 1'b1; 


/*
assign  external_write = (((pl_vcu_awaddr_axi_lite_apb == 'h41074) || (pl_vcu_awaddr_axi_lite_apb == 'h41078) || 
                           (pl_vcu_awaddr_axi_lite_apb == 'h4107C) || (pl_vcu_awaddr_axi_lite_apb == 'h41080)) 
                        && (pl_vcu_awvalid_axi_lite_apb  == 1'b1)) ? 1'b0 : 1'b1;
*/


//Write-path resp channel
assign  lc_pl_bresp_axi_lite_apb  =  ((vcu_gasket_enable == 1'b1) | (lc_bvalid == 1'b1)) ? lc_bresp : vcu_pl_bresp_axi_lite_apb ;
assign  lc_pl_bvalid_axi_lite_apb =  (wt_addr_lc_capture == 2'b11) ? 1'b0 : vcu_pl_bvalid_axi_lite_apb | lc_bvalid ;
assign  lc_vcu_bready_axi_lite_apb = (wt_addr_lc_capture == 2'b10) ? pl_vcu_bready_axi_lite_apb : 
                                     (wt_addr_lc_capture == 2'b11 && (cState == 'h3 | cState == 'h6)) ? 1'b1 : 1'b0; 

//Write-data channel
assign  lc_vcu_wvalid_axi_lite_apb =  (wt_addr_lc_capture == 2'b10) ? pl_vcu_wvalid_axi_lite_apb : 
                                      (wt_addr_lc_capture == 2'b11 && (cState == 'h2 | cState == 'h5)) ? 1'b1 : 1'b0; // lc_wready
assign  lc_vcu_wdata_axi_lite_apb  =  (wt_addr_lc_capture == 2'b10) ? pl_vcu_wdata_axi_lite_apb  : 
                                      (wt_addr_lc_capture == 2'b11 && (cState == 'h2 | cState == 'h5)) ? sm_wdata : 32'h0;
assign  lc_vcu_wstrb_axi_lite_apb  =  (wt_addr_lc_capture == 2'b10) ? pl_vcu_wstrb_axi_lite_apb  : 4'h0; 


// Need to gate the external bus lock
assign  lc_pl_wready_axi_lite_apb =  (bus_external_lock == 1'b1) ? 1'b0 :
                                     (wt_addr_lc_capture == 2'b01) ? 1'b1 : 
									 (wt_addr_lc_capture == 2'b10) ? vcu_pl_wready_axi_lite_apb : 1'b0;


assign lc_vcu_arvalid_axi_lite_apb_hm = (((pl_vcu_araddr_axi_lite_apb < 'h41000) || (pl_vcu_araddr_axi_lite_apb > 'h41084)) && 
                                          (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; // this command is for hard-macro

assign lc_vcu_arvalid_axi_lite_apb_encoder_en = ((pl_vcu_araddr_axi_lite_apb == 'h41000) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_decoder_en = ((pl_vcu_araddr_axi_lite_apb == 'h41004) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_memory_depth = ((pl_vcu_araddr_axi_lite_apb == 'h41008) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_color_depth = ((pl_vcu_araddr_axi_lite_apb == 'h4100C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_vertical_range = ((pl_vcu_araddr_axi_lite_apb == 'h41010) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_frame_size_x = ((pl_vcu_araddr_axi_lite_apb == 'h41014) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_frame_size_y = ((pl_vcu_araddr_axi_lite_apb == 'h41018) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_color_format = ((pl_vcu_araddr_axi_lite_apb == 'h4101C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_fps = ((pl_vcu_araddr_axi_lite_apb == 'h41020) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_mcu_clk = ((pl_vcu_araddr_axi_lite_apb == 'h41024) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_core_clk = ((pl_vcu_araddr_axi_lite_apb == 'h41028) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_pll_bypass = ((pl_vcu_araddr_axi_lite_apb == 'h4102C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_enc_clk = ((pl_vcu_araddr_axi_lite_apb == 'h41030) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_pll_clk = ((pl_vcu_araddr_axi_lite_apb == 'h41034) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_video_format = ((pl_vcu_araddr_axi_lite_apb == 'h41038) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_lite_apb_status = ((pl_vcu_araddr_axi_lite_apb == 'h4103C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 


assign lc_vcu_arvalid_axi_enc_clk   =  ((pl_vcu_araddr_axi_lite_apb == 'h41040) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_dec_clk   =  ((pl_vcu_araddr_axi_lite_apb == 'h41044) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_axi_mcu_clk   =  ((pl_vcu_araddr_axi_lite_apb == 'h41048) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_mcu_base_addr   =  ((pl_vcu_araddr_axi_lite_apb== 'h4104C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_mcu_range   =  ((pl_vcu_araddr_axi_lite_apb == 'h41050) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_enc0_base_addr   =  ((pl_vcu_araddr_axi_lite_apb == 'h41054) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_enc0_range   =  ((pl_vcu_araddr_axi_lite_apb == 'h41058) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_enc1_base_addr   =  ((pl_vcu_araddr_axi_lite_apb == 'h4105C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_enc1_range   =  ((pl_vcu_araddr_axi_lite_apb == 'h41060) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_dec0_base_addr   =  ((pl_vcu_araddr_axi_lite_apb == 'h41064) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
//assign lc_vcu_arvalid_dec0_range   =  ((pl_vcu_araddr_axi_lite_apb == 'h41068) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_dec0_range   =  (((pl_vcu_araddr_axi_lite_apb == 'h41068) || (pl_vcu_araddr_axi_lite_apb == 'h41074)) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_dec1_base_addr   =  ((pl_vcu_araddr_axi_lite_apb == 'h4106C) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 
assign lc_vcu_arvalid_dec1_range   =  ((pl_vcu_araddr_axi_lite_apb == 'h41070) && (pl_vcu_arvalid_axi_lite_apb  == 1'b1)) ? 1'b1 : 1'b0; 



assign sip_valid          = {lc_vcu_arvalid_axi_lite_apb_encoder_en , lc_vcu_arvalid_axi_lite_apb_decoder_en ,
                             lc_vcu_arvalid_axi_lite_apb_memory_depth , lc_vcu_arvalid_axi_lite_apb_color_depth ,
							 lc_vcu_arvalid_axi_lite_apb_vertical_range , lc_vcu_arvalid_axi_lite_apb_frame_size_x ,
                             lc_vcu_arvalid_axi_lite_apb_frame_size_y , lc_vcu_arvalid_axi_lite_apb_color_format ,
	     				     lc_vcu_arvalid_axi_lite_apb_fps ,  lc_vcu_arvalid_axi_lite_apb_mcu_clk ,
                             lc_vcu_arvalid_axi_lite_apb_core_clk , lc_vcu_arvalid_axi_lite_apb_pll_bypass,
							 lc_vcu_arvalid_axi_lite_apb_enc_clk, lc_vcu_arvalid_axi_lite_apb_pll_clk , 
							 lc_vcu_arvalid_axi_lite_apb_video_format , lc_vcu_arvalid_axi_lite_apb_status };


assign sip_addr_valid     = { lc_vcu_arvalid_axi_enc_clk, lc_vcu_arvalid_axi_dec_clk,
                              lc_vcu_arvalid_axi_mcu_clk, lc_vcu_arvalid_mcu_base_addr,
                              lc_vcu_arvalid_mcu_range, lc_vcu_arvalid_enc0_base_addr,
                              lc_vcu_arvalid_enc0_range, lc_vcu_arvalid_enc1_base_addr,
                              lc_vcu_arvalid_enc1_range, lc_vcu_arvalid_dec0_base_addr,
                              lc_vcu_arvalid_dec0_range, lc_vcu_arvalid_dec1_base_addr,
                              lc_vcu_arvalid_dec1_range };

assign soft_ip_reg_range = | sip_valid;
assign soft_ip_addr_reg_range = | sip_addr_valid;


always @ (sip_addr_valid)
begin
     case (sip_addr_valid) 
        13'b1_0000_0000_0000 :  soft_ip_addr_reg_capture = HDL_AXI_ENC_CLK;
        13'b0_1000_0000_0000 :  soft_ip_addr_reg_capture = HDL_AXI_DEC_CLK;
        13'b0_0100_0000_0000 :  soft_ip_addr_reg_capture = HDL_AXI_MCU_CLK;
        13'b0_0010_0000_0000 :  soft_ip_addr_reg_capture = HDL_DEC_VIDEO_STANDARD;
        13'b0_0001_0000_0000 :  soft_ip_addr_reg_capture = HDL_DEC_FRAME_SIZE_X;
        13'b0_0000_1000_0000 :  soft_ip_addr_reg_capture = HDL_DEC_FRAME_SIZE_Y;
        13'b0_0000_0100_0000 :  soft_ip_addr_reg_capture = HDL_DEC_FPS;
        13'b0_0000_0010_0000 :  soft_ip_addr_reg_capture = HDL_ENC_BUFFER_B_FRAME;
        13'b0_0000_0001_0000 :  soft_ip_addr_reg_capture = HDL_WPP_EN;
        13'b0_0000_0000_1000 :  soft_ip_addr_reg_capture = HDL_PLL_CLK_LO;
        13'b0_0000_0000_0100 :  soft_ip_addr_reg_capture = {30'b0, vcu_reset_capture , vcu_gasket_enable};
        13'b0_0000_0000_0010 :  soft_ip_addr_reg_capture = HDL_MAX_NUM_CORES;
        13'b0_0000_0000_0001 :  soft_ip_addr_reg_capture = {24'b0, num_concurrent_streams, num_streams};
        default              :  soft_ip_addr_reg_capture = 32'b0;
	 endcase
end


always @ (sip_valid or vcu_pl_pll_status_pll_lock or vcu_pl_pwr_supply_status_vccaux or 
          vcu_pl_pwr_supply_status_vcuint or vcu_pl_pintreq)
begin
     case (sip_valid)
        16'b1000_0000_0000_0000 :  soft_ip_reg_capture = {31'b0, HDL_ENCODER_EN};
        16'b0100_0000_0000_0000 :  soft_ip_reg_capture = {31'b0, HDL_DECODER_EN};
        16'b0010_0000_0000_0000 :  soft_ip_reg_capture = HDL_MEM_DEPTH;
        16'b0001_0000_0000_0000 :  soft_ip_reg_capture = {30'b0, HDL_COLOR_DEPTH};
        16'b0000_1000_0000_0000 :  soft_ip_reg_capture = {30'b0, HDL_ENC_BUFFER_MOTION_VEC_RANGE};
        16'b0000_0100_0000_0000 :  soft_ip_reg_capture = {20'b0, HDL_FRAME_SIZE_X};
        16'b0000_0010_0000_0000 :  soft_ip_reg_capture = {20'b0, HDL_FRAME_SIZE_Y};
        16'b0000_0001_0000_0000 :  soft_ip_reg_capture = {31'b0, HDL_COLOR_FORMAT};
        16'b0000_0000_1000_0000 :  soft_ip_reg_capture = {24'b0, HDL_FPS};
        16'b0000_0000_0100_0000 :  soft_ip_reg_capture = {22'b0, HDL_MCU_CLK};
        16'b0000_0000_0010_0000 :  soft_ip_reg_capture = {22'b0, HDL_CORE_CLK};
        16'b0000_0000_0001_0000 :  soft_ip_reg_capture = {31'b0, HDL_PLL_BYPASS};
        16'b0000_0000_0000_1000 :  soft_ip_reg_capture = {22'b0, HDL_ENC_CLK};
        16'b0000_0000_0000_0100 :  soft_ip_reg_capture = HDL_PLL_CLK_HI;
        16'b0000_0000_0000_0010 :  soft_ip_reg_capture = {31'b0, HDL_VIDEO_STANDARD};
        16'b0000_0000_0000_0001 :  soft_ip_reg_capture = {26'b0, 1'b0,1'b0, 
		                                                 vcu_pl_pll_status_pll_lock,vcu_pl_pwr_supply_status_vccaux, 
														 vcu_pl_pwr_supply_status_vcuint,vcu_pl_pintreq };
        default                 :  soft_ip_reg_capture = {32'b0 };
	 endcase
end


assign bus_free =  ((wt_addr_lc_capture == 2'b00 | wt_addr_lc_capture == 2'b11) & rd_addr_lc_capture == 2'b00) ? 1'b1 : 1'b0;
assign bus_external_lock =  disable_mcp_patch ? 1'b0 : ((bus_free == 1'b1 || (cState > 'h0)) &  trigger_pll_div_change == 1'b1 ) ? 1'b1 : 1'b0; 


// The S/M once gets a write request to Hardmacro will connect the PL to
// hardmacro. This change is done to allow pipelined access to Write.
always @ (posedge pl_vcu_axi_lite_clk) 
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
	  wt_addr_lc_capture <= 2'b00;
   end else  begin
      if ((pl_vcu_awaddr_axi_lite_apb > 'h41000) & (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         wt_addr_lc_capture <= 2'b01; // LC write/read
      end else if ((pl_vcu_awaddr_axi_lite_apb < 'h41000) & (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         wt_addr_lc_capture <= 2'b10; // HM write/read
      end else if ((nState > 'h0 )  || (cState > 'h0)) begin 
         wt_addr_lc_capture <= 2'b11; // S-M Write
      end else if (lc_bvalid == 1'b1) begin
         wt_addr_lc_capture <= 2'b00; // Reset
      end else if (vcu_pl_bvalid_axi_lite_apb == 1'b1) begin
         //wt_addr_lc_capture <= 2'b00; // Reset .. this is removed to retain
		 //connect to  HM
         wt_addr_lc_capture <= wt_addr_lc_capture;
	  end else begin
         wt_addr_lc_capture <= wt_addr_lc_capture;
	  end
   end
end


always @ (posedge pl_vcu_axi_lite_clk) 
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
	  rd_addr_lc_capture <= 2'b00;
   end else  begin
      if ((pl_vcu_araddr_axi_lite_apb > 'h41000) & (pl_vcu_arvalid_axi_lite_apb == 1'b1)) begin
         rd_addr_lc_capture <= 2'b01; // LC write/read
      end else if ((pl_vcu_araddr_axi_lite_apb < 'h41000) & (pl_vcu_arvalid_axi_lite_apb == 1'b1)) begin
         rd_addr_lc_capture <= 2'b10; // HM write/read
      end else if ((lc_pl_rvalid_axi_lite_apb &  lc_vcu_rready_axi_lite_apb) |
	               (pl_vcu_arvalid_axi_lite_apb &  vcu_pl_arready_axi_lite_apb) ) begin
         rd_addr_lc_capture <= 2'b00; // Reset
	  end else begin
         rd_addr_lc_capture <= rd_addr_lc_capture;
	  end
   end
end





always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
	  decoded_reg <= 3'b000;
   end else  begin
      if ((pl_vcu_awaddr_axi_lite_apb == 'h41074) && (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         decoded_reg <= 3'b001; //gasket&reset 
      end else if ((pl_vcu_awaddr_axi_lite_apb == 'h41078) && (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         decoded_reg <= 3'b010; //mcu_base_0 (enc)
      end else if ((pl_vcu_awaddr_axi_lite_apb == 'h4107C) && (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         decoded_reg <= 3'b011; //mcu_base_1 (enc)
      end else if ((pl_vcu_awaddr_axi_lite_apb == 'h41080) && (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         decoded_reg <= 3'b100; //mcu_base_0 (dec)
      end else if ((pl_vcu_awaddr_axi_lite_apb == 'h41084) && (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         decoded_reg <= 3'b101; //mcu_base_0 (dec)
      end else if (((pl_vcu_awaddr_axi_lite_apb < 'h41000) || (pl_vcu_awaddr_axi_lite_apb > 'h41084)) & (pl_vcu_awvalid_axi_lite_apb == 1'b1)) begin
         decoded_reg <= 3'b000; //pll_settings
	  end else begin
         decoded_reg <= decoded_reg;
	  end
   end
end


always @ (posedge pl_vcu_axi_lite_clk) 
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
	  lc_bresp <= 2'b0;
	  lc_bvalid <= 1'b0;
	  hm_bvalid <= 1'b0;
   end else  begin
	 if (wt_addr_lc_capture == 2'b01 && pl_vcu_wvalid_axi_lite_apb == 1'b1 && lc_pl_wready_axi_lite_apb == 1'b1) begin
	   lc_bresp  <= 2'b0;
	   lc_bvalid <= 1'b1;
	   hm_bvalid <= 1'b0;
	 end else if (wt_addr_lc_capture == 2'b10 && pl_vcu_wvalid_axi_lite_apb == 1'b1 && vcu_pl_wready_axi_lite_apb == 1'b1) begin
	   lc_bresp <= 2'b0;
	   lc_bvalid <= 1'b0;
	   hm_bvalid <= 1'b1;
	 end else begin
	   lc_bresp <= 2'b0;
	   lc_bvalid <= 1'b0;
	   hm_bvalid <= 1'b0;
	 end
   end
end


/*
// PLL Settings
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      pll_high_low_value <= 'h0;
   end else begin
      if ((wt_addr_lc_capture == 2'b01) && (pl_vcu_wvalid_axi_lite_apb == 1'b1) && (decoded_reg == 3'b100))
	  begin
          pll_high_low_value  <= pl_vcu_wdata_axi_lite_apb[15:0];
	  end else 
	  begin
          pll_high_low_value  <= pll_high_low_value;
	  end
   end
end
*/


// MCU_base_address low
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      mcu_wt_snoop_addr_pll_change_low_enc <= 'h0;
   end else begin
      if ((wt_addr_lc_capture == 2'b01) && (pl_vcu_wvalid_axi_lite_apb == 1'b1) && (decoded_reg == 3'b010))
	  begin
          mcu_wt_snoop_addr_pll_change_low_enc  <= pl_vcu_wdata_axi_lite_apb[31:0];
	  end else begin
          mcu_wt_snoop_addr_pll_change_low_enc <= mcu_wt_snoop_addr_pll_change_low_enc ;
	  end
   end
end
// MCU_base_address low
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      mcu_wt_snoop_addr_pll_change_low_dec <= 'h0;
   end else begin
      if ((wt_addr_lc_capture == 2'b01) && (pl_vcu_wvalid_axi_lite_apb == 1'b1) && (decoded_reg == 3'b100))
	  begin
          mcu_wt_snoop_addr_pll_change_low_dec  <= pl_vcu_wdata_axi_lite_apb[31:0];
	  end else begin
          mcu_wt_snoop_addr_pll_change_low_dec <= mcu_wt_snoop_addr_pll_change_low_dec ;
	  end
   end
end


// MCU_base_address high
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      mcu_wt_snoop_addr_pll_change_hi_enc  <= 'h0;
   end else begin
      if ((wt_addr_lc_capture == 2'b01) && (pl_vcu_wvalid_axi_lite_apb == 1'b1) && (decoded_reg == 3'b011))
	  begin
          mcu_wt_snoop_addr_pll_change_hi_enc   <= pl_vcu_wdata_axi_lite_apb[11:0];
	  end else begin
          mcu_wt_snoop_addr_pll_change_hi_enc   <= mcu_wt_snoop_addr_pll_change_hi_enc  ;
	  end
   end
end

// MCU_base_address high
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      mcu_wt_snoop_addr_pll_change_hi_dec  <= 'h0;
   end else begin
      if ((wt_addr_lc_capture == 2'b01) && (pl_vcu_wvalid_axi_lite_apb == 1'b1) && (decoded_reg == 3'b101))
	  begin
          mcu_wt_snoop_addr_pll_change_hi_dec   <= pl_vcu_wdata_axi_lite_apb[11:0];
	  end else begin
          mcu_wt_snoop_addr_pll_change_hi_dec   <= mcu_wt_snoop_addr_pll_change_hi_dec  ;
	  end
   end
end

// Reset and Gasket registers..
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      vcu_gasket_enable <= 1'b1;
      vcu_reset_capture <= 1'b0;
      disable_mcp_patch <= 1'b1;
   end else begin
      if ((wt_addr_lc_capture == 2'b01) && ( pl_vcu_wvalid_axi_lite_apb == 1'b1 ) && (decoded_reg == 3'b001))
	  begin
          vcu_gasket_enable          <= (!pl_vcu_wdata_axi_lite_apb[0:0]);
          vcu_reset_capture          <=   pl_vcu_wdata_axi_lite_apb[1:1] & pl_vcu_raw_rst_n;
          //disable_mcp_patch          <= !(pl_vcu_wdata_axi_lite_apb[31:31]);
          disable_mcp_patch          <= 1'b1; // Should add this chicken bit to a new register..

          //vcu_pll_test_sel           <= pl_vcu_wdata_axi_lite_apb[31:28];//4'b1111;
          //vcu_pll_test_ck_sel        <= pl_vcu_wdata_axi_lite_apb[26:24];//3'b111;
          //vcu_pll_test_fract_en      <= pl_vcu_wdata_axi_lite_apb[21:21];//1'b0;
          //vcu_pll_test_fract_clk_sel <= pl_vcu_wdata_axi_lite_apb[20:20];//1'b1;
	  end else 
	  begin
          vcu_gasket_enable          <= vcu_gasket_enable;
          vcu_reset_capture          <= vcu_reset_capture & pl_vcu_raw_rst_n;
          disable_mcp_patch          <= disable_mcp_patch;
	  end
   end
end

// Read path registers
always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
    soft_ip_reg_capture_r <= 32'b0;
   	sip_rresp_r  <= 2'b00;
   	sip_rvalid_r <= 1'b0;
   end else 
   if (soft_ip_reg_range == 1'b1) begin
        soft_ip_reg_capture_r <= soft_ip_reg_capture;
		sip_rresp_r  <= 2'b00;
		sip_rvalid_r <= 1'b1;
   end else 
   if (soft_ip_addr_reg_range == 1'b1) begin
        soft_ip_reg_capture_r <= soft_ip_addr_reg_capture;
		sip_rresp_r  <= 2'b00;
		sip_rvalid_r <= 1'b1;
   end else 
   if  ((pl_vcu_rready_axi_lite_apb == 1'b0) && (sip_rvalid_r == 1'b1)) begin
	    soft_ip_reg_capture_r <= soft_ip_reg_capture_r;
		sip_rresp_r  <= 2'b00;
		sip_rvalid_r <= 1'b1;
   end else begin
        soft_ip_reg_capture_r <= 32'b0;
		sip_rresp_r  <= 2'b00;
		sip_rvalid_r <= 1'b0;
   end
end

// Mux the signals from hard-macro and softip  

assign lc_pl_rdata_axi_lite_apb   = (sip_rvalid_r == 1'b1) ? soft_ip_reg_capture_r : vcu_pl_rdata_axi_lite_apb;
assign lc_pl_rresp_axi_lite_apb   = (sip_rvalid_r == 1'b1) ? sip_rresp_r : vcu_pl_rresp_axi_lite_apb;
// Need to gate the external bus lock
assign lc_pl_rvalid_axi_lite_apb  = (bus_external_lock == 1'b1) ? 1'b0 : (sip_rvalid_r == 1'b1) ? 1'b1 :vcu_pl_rvalid_axi_lite_apb;
assign lc_vcu_rready_axi_lite_apb = pl_vcu_rready_axi_lite_apb;


always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      cState <= 'h0 ; 
   end else begin
      cState <= nState;
   end
end


always @ (posedge pl_vcu_axi_lite_clk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
      pll_div_change_done_reg <= 'b0 ; 
   end else begin
	  if ( pll_div_change_done &  trigger_pll_div_change ) begin
         pll_div_change_done_reg <=  1'b1;
	  end else begin
	      if (trigger_pll_div_change == 1'b1) begin
             pll_div_change_done_reg <= pll_div_change_done_reg;
		  end else begin   
             pll_div_change_done_reg <=  1'b0;
		  end
	  end
   end
end

assign addr_w_done = lc_vcu_awvalid_axi_lite_apb & vcu_pl_awready_axi_lite_apb ;
assign data_w_done = lc_vcu_wvalid_axi_lite_apb & vcu_pl_wready_axi_lite_apb ;
assign data_w_resp = lc_vcu_bready_axi_lite_apb & vcu_pl_bvalid_axi_lite_apb ;

always @ (*) 
begin
   nState = cState;
   sm_waddr = 'h0; 
   sm_wdata = 'h0; 
   pll_div_change_done = 1'b0; 
   case (cState)
      'h0 : begin
         if (bus_external_lock) begin
            nState = 'h1;
		 end
	  end
	  'h1 : begin // state for Enc Write addr
	      sm_waddr = 'h40030; 
	      sm_wdata = pll_div_value; 
		  if (addr_w_done == 1'b1) begin
            nState = 'h2;
		  end
	  end
	  'h2 : begin // state for Enc Write data
	      sm_waddr = 'h40030; 
	      sm_wdata = pll_div_value; 
		  if (data_w_done== 1'b1) begin
            nState = 'h3;
		  end
	  end
	  'h3 : begin // state for Enc Write resp
	      sm_waddr = 'h40030; 
	      sm_wdata = pll_div_value; 
		  if (data_w_resp == 1'b1) begin
            nState = 'h4;
		  end
	  end
	  'h4 : begin // state for Dec Write addr
	      sm_waddr = 'h40038; 
	      sm_wdata = pll_div_value; 
		  if (addr_w_done == 1'b1) begin
            nState = 'h5;
		  end
	  end
	  'h5 : begin // state for Dec Write data
	      sm_waddr = 'h40038; 
	      sm_wdata = pll_div_value; 
		  if (data_w_done== 1'b1) begin
            nState = 'h6;
		  end
	  end
	  'h6 : begin // state for Dec Write resp
	      sm_waddr = 'h40038; 
	      sm_wdata = pll_div_value; 
		  if (data_w_resp == 1'b1) begin
            nState = 'h0;
            pll_div_change_done = 1'b1; 
		  end
	  end
	  default : begin
         nState = 'h0;
	  end
   endcase 
end

generate 
    if ((HDL_TEST_PORT_EN == 1) || (HDL_VCU_TEST_EN == 1)) begin
    vcu_v1_2_1_mcp_patch  #(
    
                          )  mcp_patch  (
    
                            .pl_vcu_axi_lite_clk(pl_vcu_axi_lite_clk),
    	                    .m_axi_mcu_aclk(m_axi_mcu_aclk),
                            .pl_vcu_raw_rst_n(pl_vcu_raw_rst_n),
                            .vcu_resetn_soft(vcu_resetn_soft),
    
                            .pll_div_change_done(pll_div_change_done_reg),
                            .trigger_pll_div_change(trigger_pll_div_change),
                            .pll_div_value(pll_div_value),
    
                            .vcu_pl_mcu_m_axi_ic_dc_awaddr(vcu_pl_mcu_m_axi_ic_dc_awaddr), 
                            .vcu_pl_mcu_m_axi_ic_dc_awvalid(vcu_pl_mcu_m_axi_ic_dc_awvalid),
                            .pl_vcu_mcu_m_axi_ic_dc_awready(pl_vcu_mcu_m_axi_ic_dc_awready),
    
    						.clock_high_enable(clock_high_enable ),
    						.clock_low_enable (clock_low_enable ),
                            
                            .mcu_wt_snoop_addr_pll_change_enc(mcu_wt_snoop_addr_pll_change_enc),
                            .mcu_wt_snoop_addr_pll_change_dec(mcu_wt_snoop_addr_pll_change_dec)
                            //.pll_high_low_value(pll_high_low_value)
    
    					  );
    end else begin

       assign   trigger_pll_div_change = 1'b0;
	   assign   pll_div_value  = 'h0;
	   assign   clock_high_enable = 1'b0;
	   assign   clock_low_enable = 1'b0; 

	end
    
endgenerate


endmodule



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
// Filename:    vcu_v1_2_1_mcp_patch
// Version:       v1.00.a
// Description:   This is the softIP for mcp patch
//-----------------------------------------------------------------------------
// Structure:   This section shows the hierarchical structure of 
//              vcu_wrapper.
//
//              --vcu_v1_2_1_vcu.v
//                    --VCU.v - Unisim component
//                    --vcu_v1_2_1_registers.v
//                    --vcu_v1_2_1_mcp_patch.v
//-----------------------------------------------------------------------------
// Author:      kvikrama
//
// History:
//
//------------------------------------------------------------------------------


module vcu_v1_2_1_mcp_patch
#(

parameter STATE_INIT = 0,
parameter STATE_PLL_LOW = 1,
parameter STATE_PLL_HIGH = 2,
parameter STATE_PLACEHOLDER = 7

)
(

//VCU-AXI-SLAVE-External-Bus-In
input wire                                pl_vcu_axi_lite_clk,
input wire                                m_axi_mcu_aclk,
input wire                                pl_vcu_raw_rst_n,
input wire                                vcu_resetn_soft,

input wire [43:0]                         vcu_pl_mcu_m_axi_ic_dc_awaddr, 
input wire                                vcu_pl_mcu_m_axi_ic_dc_awvalid,
input wire                                pl_vcu_mcu_m_axi_ic_dc_awready,

input                                     pll_div_change_done,
output                                    trigger_pll_div_change,
output [31:0]                             pll_div_value,

output                                    clock_high_enable,
output                                    clock_low_enable,

input wire [43:0]                         mcu_wt_snoop_addr_pll_change_dec,
input wire [43:0]                         mcu_wt_snoop_addr_pll_change_enc
//input wire [15:0]                       pll_high_low_value

);


reg        trigger_pll_div_change ;
reg [31:0] pll_div_value;
reg [2:0]  state_mc, next_state_mc;
reg        clock_high_enable, clock_low_enable;
reg        clock_high_enable_pre, clock_low_enable_pre;


always @ (posedge m_axi_mcu_aclk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
       clock_high_enable <= 'b0; 
   end else begin
       clock_high_enable <= clock_high_enable_pre; 
   end
end

always @ (posedge m_axi_mcu_aclk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
       clock_low_enable <= 'b0; 
   end else begin
       clock_low_enable <= clock_low_enable_pre; 
   end
end



always @ (posedge m_axi_mcu_aclk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
       state_mc <= STATE_PLL_HIGH; 
   end else begin
       state_mc <= next_state_mc; 
   end
end

always @ ( *  )
begin
    next_state_mc =  state_mc;
    pll_div_value = 'h1041; // 667 mhz
    clock_high_enable_pre = clock_high_enable;
	clock_low_enable_pre = clock_low_enable;
	case (state_mc)
	    STATE_PLL_LOW : begin
          if ((mcu_wt_snoop_addr_pll_change_enc == vcu_pl_mcu_m_axi_ic_dc_awaddr) &
	          (vcu_pl_mcu_m_axi_ic_dc_awvalid == 1) & (pl_vcu_mcu_m_axi_ic_dc_awready == 1)) 
	      begin
               next_state_mc = STATE_PLL_HIGH;
               pll_div_value = 'h1021; // 667 mhz
               clock_high_enable_pre = 'b1;
               clock_low_enable_pre = 'b0;
		  end
        end
	    STATE_PLL_HIGH : begin
          if ((mcu_wt_snoop_addr_pll_change_enc == vcu_pl_mcu_m_axi_ic_dc_awaddr) &
	          (vcu_pl_mcu_m_axi_ic_dc_awvalid == 1) & (pl_vcu_mcu_m_axi_ic_dc_awready == 1)) 
	      begin
               next_state_mc = STATE_PLL_LOW;
               pll_div_value = 'h1041; // 333 mhz
               clock_high_enable_pre = 'b0;
               clock_low_enable_pre = 'b1;
		  end
        end
	    default : begin
               next_state_mc = STATE_PLL_HIGH;
               pll_div_value = 'h1021; // 333 mhz
        end
    endcase 
end


always @ (posedge m_axi_mcu_aclk)
begin
   if (pl_vcu_raw_rst_n == 1'b0) begin
       trigger_pll_div_change <= 1'b0; 
   end else begin
      if ((mcu_wt_snoop_addr_pll_change_enc == vcu_pl_mcu_m_axi_ic_dc_awaddr) &
	      (vcu_pl_mcu_m_axi_ic_dc_awvalid == 1) & (pl_vcu_mcu_m_axi_ic_dc_awready == 1)) 
	  begin
          trigger_pll_div_change <= 1'b1;
	  end else begin
	      if (pll_div_change_done  == 1'b1) 
             trigger_pll_div_change <= 1'b0;
		  else 
             trigger_pll_div_change <= trigger_pll_div_change;
	  end
   end
end


endmodule


