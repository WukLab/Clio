// -- (c) Copyright 2010 - 2014 Xilinx, Inc. All rights reserved.
// --
// -- This file contains confidential and proprietary information
// -- of Xilinx, Inc. and is protected under U.S. and 
// -- international copyright and other intellectual property
// -- laws.
// --
// -- DISCLAIMER
// -- This disclaimer is not a license and does not grant any
// -- rights to the materials distributed herewith. Except as
// -- otherwise provided in a valid license issued to you by
// -- Xilinx, and to the maximum extent permitted by applicable
// -- law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// -- WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// -- AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// -- BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// -- INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// -- (2) Xilinx shall not be liable (whether in contract or tort,
// -- including negligence, or under any other theory of
// -- liability) for any loss or damage of any kind or nature
// -- related to, arising under or in connection with these
// -- materials, including for any direct, or any indirect,
// -- special, incidental, or consequential loss or damage
// -- (including loss of data, profits, goodwill, or any type of
// -- loss or damage suffered as a result of any action brought
// -- by a third party) even if such damage or loss was
// -- reasonably foreseeable or Xilinx had been advised of the
// -- possibility of the same.
// --
// -- CRITICAL APPLICATIONS
// -- Xilinx products are not designed or intended to be fail-
// -- safe, or for use in any application requiring fail-safe
// -- performance, such as life-support or safety devices or
// -- systems, Class III medical devices, nuclear facilities,
// -- applications related to the deployment of airbags, or any
// -- other applications that could lead to death, personal
// -- injury, or severe property or environmental damage
// -- (individually and collectively, "Critical
// -- Applications"). Customer assumes the sole risk and
// -- liability of any use of Xilinx products in Critical
// -- Applications, subject only to applicable laws and
// -- regulations governing limitations on product liability.
// --
// -- THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// -- PART OF THIS FILE AT ALL TIMES.
//-----------------------------------------------------------------------------
//
// Description: Addr Decoder
// Each received address is compared to a list of address ranges (base, size). 
// The matching range's index (if any) is output combinatorially.
// If the decode is successful, the MATCH output is asserted.
//
// Verilog-standard:  Verilog 2001
//--------------------------------------------------------------------------
//
// Structure:
//   addr_decoder
//
//--------------------------------------------------------------------------
`timescale 1ps/1ps

(* DowngradeIPIdentifiedWarnings="yes" *) 
module axi_mmu_v2_1_17_addr_decoder #
  (
   parameter                       C_FAMILY         = "rtl",
   parameter integer               C_NUM_RANGES     = 1,                        // Number of address ranges [1..256]
   parameter integer               C_NUM_RANGES_LOG = 1,                        // Width of matching range index (min 1)
   parameter integer               C_ADDR_WIDTH     = 32,                       // Width of address operand [2:64]
   parameter [C_NUM_RANGES*64-1:0] C_BASE_ADDR      = {C_NUM_RANGES*64{1'b1}},  // Aligned to 2**C_RANGE_SIZE
   parameter [C_NUM_RANGES*32-1:0] C_RANGE_SIZE     = {C_NUM_RANGES*32{1'b0}},  // Binary power of range size [0=null_range, 1..C_ADDR_WIDTH]
   parameter [C_NUM_RANGES:0]      C_RANGE_QUAL     = {C_NUM_RANGES{1'b1}}      // Range enabled for this decoder instance
   )
  (
   input  wire [C_ADDR_WIDTH-1:0]      addr,        // Decoder input operand
   output wire [C_NUM_RANGES_LOG-1:0]  range_enc,   // Index of matching address range (encoded)
   output wire                         match        // Decode successful
   );
  
  // Generate Variables
  genvar rng;
  
  // Functions
  
  // Function to detect match of one addressable range (returns Boolean).
  function  decode_address (
      input [C_ADDR_WIDTH-1:0] base,
      input [31:0]             size,
      input [C_ADDR_WIDTH-1:0] addr
    );
    integer i;
    begin
      if (size == 32'b0) begin  // null range
        decode_address = 1'b0;
      end else begin
        decode_address = 1'b1;
        for (i = size; i < C_ADDR_WIDTH; i = i + 1) begin
          decode_address = decode_address & ~(addr[i] ^ base[i]);
        end
      end
    end
  endfunction
  
  // Translate one-hot to binary encoded
  function [C_NUM_RANGES_LOG-1:0] f_hot2enc
    (
      input [C_NUM_RANGES-1:0]  one_hot
    );
    integer i;
    integer j;
    begin
      for (i=0; i<C_NUM_RANGES_LOG; i=i+1) begin
        f_hot2enc[i] = 1'b0;
        for (j=0; j<C_NUM_RANGES; j=j+1) begin
          f_hot2enc[i] = f_hot2enc[i] | (j[i] & one_hot[j]);
        end
      end
    end
  endfunction

  wire [C_NUM_RANGES-1:0]               range_hot;        // Range matching address (1-hot).

  generate
  
    for (rng = 0; rng < C_NUM_RANGES; rng = rng + 1) begin : gen_rng
      assign range_hot[rng] = C_RANGE_QUAL[rng] ? 
        decode_address(C_BASE_ADDR[rng*64 +: C_ADDR_WIDTH], C_RANGE_SIZE[rng*32 +: 32], addr)
        : 1'b0;
    end
  
    assign match = |range_hot;
    assign range_enc = f_hot2enc(range_hot);
    
  endgenerate
  
endmodule


// -- (c) Copyright 2009 - 2014 Xilinx, Inc. All rights reserved.
// --
// -- This file contains confidential and proprietary information
// -- of Xilinx, Inc. and is protected under U.S. and 
// -- international copyright and other intellectual property
// -- laws.
// --
// -- DISCLAIMER
// -- This disclaimer is not a license and does not grant any
// -- rights to the materials distributed herewith. Except as
// -- otherwise provided in a valid license issued to you by
// -- Xilinx, and to the maximum extent permitted by applicable
// -- law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// -- WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// -- AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// -- BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// -- INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// -- (2) Xilinx shall not be liable (whether in contract or tort,
// -- including negligence, or under any other theory of
// -- liability) for any loss or damage of any kind or nature
// -- related to, arising under or in connection with these
// -- materials, including for any direct, or any indirect,
// -- special, incidental, or consequential loss or damage
// -- (including loss of data, profits, goodwill, or any type of
// -- loss or damage suffered as a result of any action brought
// -- by a third party) even if such damage or loss was
// -- reasonably foreseeable or Xilinx had been advised of the
// -- possibility of the same.
// --
// -- CRITICAL APPLICATIONS
// -- Xilinx products are not designed or intended to be fail-
// -- safe, or for use in any application requiring fail-safe
// -- performance, such as life-support or safety devices or
// -- systems, Class III medical devices, nuclear facilities,
// -- applications related to the deployment of airbags, or any
// -- other applications that could lead to death, personal
// -- injury, or severe property or environmental damage
// -- (individually and collectively, "Critical
// -- Applications"). Customer assumes the sole risk and
// -- liability of any use of Xilinx products in Critical
// -- Applications, subject only to applicable laws and
// -- regulations governing limitations on product liability.
// --
// -- THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// -- PART OF THIS FILE AT ALL TIMES.
//-----------------------------------------------------------------------------
//
// File name: decerr_slave.v
//
// Description: 
//   Phantom slave interface used to complete W, R and B channel transfers when an
//   erroneous transaction is trapped.
//--------------------------------------------------------------------------
//
// Structure:
//    decerr_slave
//    
//-----------------------------------------------------------------------------

`timescale 1ps/1ps
`default_nettype none

(* DowngradeIPIdentifiedWarnings="yes" *) 
module axi_mmu_v2_1_17_decerr_slave #
  (
   parameter integer C_AXI_ID_WIDTH           = 1,
   parameter integer C_AXI_DATA_WIDTH         = 32,
   parameter integer C_AXI_BUSER_WIDTH        = 1,
   parameter integer C_AXI_RUSER_WIDTH        = 1,
   parameter integer C_AXI_PROTOCOL           = 0,
   parameter integer C_AXI_SUPPORTS_WRITE     = 1,
   parameter integer C_AXI_SUPPORTS_READ      = 1,
   parameter integer C_RESP                   = 2'b11  // DECERR
   )
  (
   input  wire                          aclk,
   input  wire                          aresetn,
   input  wire [(C_AXI_ID_WIDTH-1):0]   s_axi_awid,
   input  wire                          s_axi_awvalid,
   output wire                          s_axi_awready,
   input  wire                          s_axi_wlast,
   input  wire                          s_axi_wvalid,
   output wire                          s_axi_wready,
   output wire [(C_AXI_ID_WIDTH-1):0]   s_axi_bid,
   output wire [1:0]                    s_axi_bresp,
   output wire [C_AXI_BUSER_WIDTH-1:0]  s_axi_buser,
   output wire                          s_axi_bvalid,
   input  wire                          s_axi_bready,
   input  wire [(C_AXI_ID_WIDTH-1):0]   s_axi_arid,
   input  wire [7:0]                    s_axi_arlen,
   input  wire                          s_axi_arvalid,
   output wire                          s_axi_arready,
   output wire [(C_AXI_ID_WIDTH-1):0]   s_axi_rid,
   output wire [(C_AXI_DATA_WIDTH-1):0] s_axi_rdata,
   output wire [1:0]                    s_axi_rresp,
   output wire [C_AXI_RUSER_WIDTH-1:0]  s_axi_ruser,
   output wire                          s_axi_rlast,
   output wire                          s_axi_rvalid,
   input  wire                          s_axi_rready
   );
   
  localparam P_WRITE_IDLE = 2'b00;
  localparam P_WRITE_DATA = 2'b01;
  localparam P_WRITE_RESP = 2'b10;
  localparam P_READ_IDLE = 1'b0;
  localparam P_READ_DATA = 1'b1;
  localparam integer  P_AXI4 = 0;
  localparam integer  P_AXI3 = 1;
  localparam integer  P_AXILITE = 2;
   
  assign s_axi_rdata   = {C_AXI_DATA_WIDTH{1'b0}};
  assign s_axi_buser   = {C_AXI_BUSER_WIDTH{1'b0}};
  assign s_axi_ruser   = {C_AXI_RUSER_WIDTH{1'b0}};
  
  generate
  
  if (C_AXI_PROTOCOL == P_AXILITE) begin : gen_axilite
    
    assign s_axi_rlast = 1'b1;
    assign s_axi_bid = 0;
    assign s_axi_rid = 0;
    
    if (C_AXI_SUPPORTS_WRITE) begin : gen_write
      
      reg s_axi_awready_i = 1'b0;
      reg s_axi_wready_i = 1'b0;
      reg s_axi_bvalid_i = 1'b0;
      
      assign s_axi_bresp   = C_RESP;
      assign s_axi_awready = s_axi_awready_i;
      assign s_axi_wready  = s_axi_wready_i;
      assign s_axi_bvalid  = s_axi_bvalid_i;
      
      always @(posedge aclk) begin
        if (~aresetn) begin
          s_axi_awready_i <= 1'b0;
          s_axi_wready_i <= 1'b0;
          s_axi_bvalid_i <= 1'b0;
        end else begin
          if (s_axi_bvalid_i) begin
            if (s_axi_bready) begin
              s_axi_bvalid_i <= 1'b0;
            end
          end else if (s_axi_awvalid & s_axi_wvalid) begin
            if (s_axi_awready_i) begin
              s_axi_awready_i <= 1'b0;
              s_axi_wready_i <= 1'b0;
              s_axi_bvalid_i <= 1'b1;
            end else begin
              s_axi_awready_i <= 1'b1;
              s_axi_wready_i <= 1'b1;
            end
          end
        end
      end
      
    end else begin : gen_w_tieoff
      
      assign s_axi_bresp   = 2'b0;
      assign s_axi_awready = 1'b0;
      assign s_axi_wready  = 1'b0;
      assign s_axi_bvalid  = 1'b0;
      
    end  // gen_write
             
    if (C_AXI_SUPPORTS_READ) begin : gen_read
      
      reg s_axi_arready_i = 1'b0;
      reg s_axi_rvalid_i = 1'b0;

      assign s_axi_rresp   = C_RESP;
      assign s_axi_arready = s_axi_arready_i;
      assign s_axi_rvalid  = s_axi_rvalid_i;
      
      always @(posedge aclk) begin
        if (~aresetn) begin
          s_axi_arready_i <= 1'b0;
          s_axi_rvalid_i <= 1'b0;
        end else begin
          if (s_axi_rvalid_i) begin
            if (s_axi_rready) begin
              s_axi_rvalid_i <= 1'b0;
            end
          end else if (s_axi_arvalid & s_axi_arready_i) begin
            s_axi_arready_i <= 1'b0;
            s_axi_rvalid_i <= 1'b1;
          end else begin
            s_axi_arready_i <= 1'b1;
          end
        end
      end
        
    end else begin : gen_r_tieoff
      
      assign s_axi_rresp   = 2'b0;
      assign s_axi_arready = 1'b0;
      assign s_axi_rvalid  = 1'b0;
      
    end  // gen_read
    
  end else begin : gen_axi
  
    if (C_AXI_SUPPORTS_WRITE) begin : gen_write
      
      reg s_axi_awready_i = 1'b0;
      reg s_axi_wready_i = 1'b0;
      reg s_axi_bvalid_i = 1'b0;
      reg [(C_AXI_ID_WIDTH-1):0] s_axi_bid_i;
      reg [1:0] write_cs = P_WRITE_IDLE;
    
      assign s_axi_bresp   = C_RESP;
      assign s_axi_awready = s_axi_awready_i;
      assign s_axi_wready  = s_axi_wready_i;
      assign s_axi_bvalid  = s_axi_bvalid_i;
      assign s_axi_bid = s_axi_bid_i;
    
      always @(posedge aclk) begin
        if (~aresetn) begin
          write_cs <= P_WRITE_IDLE;
          s_axi_awready_i <= 1'b0;
          s_axi_wready_i <= 1'b0;
          s_axi_bvalid_i <= 1'b0;
          s_axi_bid_i <= 0;
        end else begin
          case (write_cs) 
            P_WRITE_IDLE: 
              begin
                if (s_axi_awvalid & s_axi_awready_i) begin
                  s_axi_awready_i <= 1'b0;
                  s_axi_bid_i <= s_axi_awid;
                  s_axi_wready_i <= 1'b1;
                  write_cs <= P_WRITE_DATA;
                end else begin
                  s_axi_awready_i <= 1'b1;
                end
              end
            P_WRITE_DATA:
              begin
                if (s_axi_wvalid & s_axi_wlast) begin
                  s_axi_wready_i <= 1'b0;
                  s_axi_bvalid_i <= 1'b1;
                  write_cs <= P_WRITE_RESP;
                end
              end
            P_WRITE_RESP:
              begin
                if (s_axi_bready) begin
                  s_axi_bvalid_i <= 1'b0;
                  s_axi_awready_i <= 1'b1;
                  write_cs <= P_WRITE_IDLE;
                end
              end
          endcase
        end
      end

    end else begin : gen_w_tieoff
      
      assign s_axi_bresp   = 2'b0;
      assign s_axi_awready = 1'b0;
      assign s_axi_wready  = 1'b0;
      assign s_axi_bvalid  = 1'b0;
      assign s_axi_bid = 0;
      
    end  // gen_write

    if (C_AXI_SUPPORTS_READ) begin : gen_read
      
      reg s_axi_arready_i = 1'b0;
      reg s_axi_rvalid_i = 1'b0;
      reg [7:0] read_cnt;
      reg s_axi_rlast_i;
      reg [(C_AXI_ID_WIDTH-1):0] s_axi_rid_i;
      reg [0:0] read_cs = P_READ_IDLE;

      assign s_axi_rresp   = C_RESP;
      assign s_axi_arready = s_axi_arready_i;
      assign s_axi_rvalid  = s_axi_rvalid_i;
      assign s_axi_rlast = s_axi_rlast_i;
      assign s_axi_rid = s_axi_rid_i;
      
      always @(posedge aclk) begin
        if (~aresetn) begin
          read_cs <= P_READ_IDLE;
          s_axi_arready_i <= 1'b0;
          s_axi_rvalid_i <= 1'b0;
          s_axi_rlast_i <= 1'b0;
          s_axi_rid_i <= 0;
          read_cnt <= 0;
        end else begin
          case (read_cs) 
            P_READ_IDLE: 
              begin
                if (s_axi_arvalid & s_axi_arready_i) begin
                  s_axi_arready_i <= 1'b0;
                  s_axi_rid_i <= s_axi_arid;
                  read_cnt <= s_axi_arlen;
                  s_axi_rvalid_i <= 1'b1;
                  if (s_axi_arlen == 0) begin
                    s_axi_rlast_i <= 1'b1;
                  end else begin
                    s_axi_rlast_i <= 1'b0;
                  end
                  read_cs <= P_READ_DATA;
                end else begin
                  s_axi_arready_i <= 1'b1;
                end
              end
            P_READ_DATA:
              begin
                if (s_axi_rready) begin
                  if (read_cnt == 0) begin
                    s_axi_rvalid_i <= 1'b0;
                    s_axi_rlast_i <= 1'b0;
                    s_axi_arready_i <= 1'b1;
                    read_cs <= P_READ_IDLE;
                  end else begin
                    if (read_cnt == 1) begin
                      s_axi_rlast_i <= 1'b1;
                    end
                    read_cnt <= read_cnt - 1;
                  end
                end
              end
          endcase
        end
      end
  
    end else begin : gen_r_tieoff
      
      assign s_axi_rresp   = 2'b0;
      assign s_axi_arready = 1'b0;
      assign s_axi_rvalid  = 1'b0;
      assign s_axi_rlast = 1'b1;
      assign s_axi_rid = 0;
      
    end  // gen_read
    
  end  // gen_axi
  endgenerate

endmodule

`default_nettype wire


//  (c) Copyright 2014 Xilinx, Inc. All rights reserved.
//
//  This file contains confidential and proprietary information
//  of Xilinx, Inc. and is protected under U.S. and
//  international copyright and other intellectual property
//  laws.
//
//  DISCLAIMER
//  This disclaimer is not a license and does not grant any
//  rights to the materials distributed herewith. Except as
//  otherwise provided in a valid license issued to you by
//  Xilinx, and to the maximum extent permitted by applicable
//  law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
//  WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
//  AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
//  BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
//  INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
//  (2) Xilinx shall not be liable (whether in contract or tort,
//  including negligence, or under any other theory of
//  liability) for any loss or damage of any kind or nature
//  related to, arising under or in connection with these
//  materials, including for any direct, or any indirect,
//  special, incidental, or consequential loss or damage
//  (including loss of data, profits, goodwill, or any type of
//  loss or damage suffered as a result of any action brought
//  by a third party) even if such damage or loss was
//  reasonably foreseeable or Xilinx had been advised of the
//  possibility of the same.
//
//  CRITICAL APPLICATIONS
//  Xilinx products are not designed or intended to be fail-
//  safe, or for use in any application requiring fail-safe
//  performance, such as life-support or safety devices or
//  systems, Class III medical devices, nuclear facilities,
//  applications related to the deployment of airbags, or any
//  other applications that could lead to death, personal
//  injury, or severe property or environmental damage
//  (individually and collectively, "Critical
//  Applications"). Customer assumes the sole risk and
//  liability of any use of Xilinx products in Critical
//  Applications, subject only to applicable laws and
//  regulations governing limitations on product liability.
//
//  THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
//  PART OF THIS FILE AT ALL TIMES. 
//-----------------------------------------------------------------------------
//
// AXI MMU
//   Virtual-to-physical address mapping with master-specific address range checking.
//
// Verilog-standard:  Verilog 2001
//
// Performance requirements:
// 1. Pre-assert SI A*READY (from reg-slice SI) to avoid stall cycles.
// 2. Support 50% duty cycle on A* handshake (while addresses hit).
// 3. Propagate MI WVALID on cycle following SI AWVALID with address hit (if SI WVALID asserted).
// 4. Delay DECERR response until all outstanding transactions have completed.
// 5. Stall when valid transaction counter reaches 32.
// 6. No combinatorial outputs based on address decode.
// 7. One cycle latency from SI to MI on AW/AR channels.
//
//--------------------------------------------------------------------------
//
// Structure:
//   axi_mmu
//     addr_decoder
//     decerr_slave
//
//--------------------------------------------------------------------------

`timescale 1ps/1ps

(* DowngradeIPIdentifiedWarnings="yes" *) 
module axi_mmu_v2_1_17_top #
  (
   parameter C_FAMILY                            = "rtl",
   parameter integer C_AXI_PROTOCOL              = 0,
             // 0 = "AXI4", 1 = "AXI3", 2 = "AXI4LITE"
   parameter integer C_AXI_ID_WIDTH              = 1,
             // Range: 1..32
   parameter integer C_S_AXI_ADDR_WIDTH          = 32,
             // Range: 1..64
   parameter integer C_M_AXI_ADDR_WIDTH          = 32,
             // Range: C_S_AXI_ADDR_WIDTH..64
   parameter integer C_AXI_DATA_WIDTH            = 32,
             // Range: 32, 64, 128, 256, 512, 1024
   parameter integer C_AXI_SUPPORTS_USER_SIGNALS = 0,
             // 0 = No; 1 = Yes
   parameter integer C_AXI_AWUSER_WIDTH          = 1,
             // Range: 1..1024
   parameter integer C_AXI_ARUSER_WIDTH          = 1,
             // Range: 1..1024
   parameter integer C_AXI_WUSER_WIDTH           = 1,
             // Range: 1..1024
   parameter integer C_AXI_RUSER_WIDTH           = 1,
             // Range: 1..1024
   parameter integer C_AXI_BUSER_WIDTH           = 1,
             // Range: 1..1024
   parameter integer C_NUM_RANGES                = 1, 
             // Total number of address ranges to be decoded
             // Range: 1..256
   parameter [C_NUM_RANGES*64-1:0] C_BASE_ADDR   = 64'h0000000000000000,
             // Base address of each range
             // Range per element: 0..(2**C_S_AXI_ADDR_WIDTH-1)
             // Concatenation of user parameters Dddd_BASE_ADDR
   parameter [C_NUM_RANGES*32-1:0] C_RANGE_SIZE  = 64'h00000010,
             // Range per element: 0 (null range), 1..C_S_AXI_ADDR_WIDTH
             // Binary power of the size of each range (number of low-order ADDR bits that can vary within the range)
             // Concatenation of user parameters Dddd_ADDR_WIDTH
   parameter integer C_USES_DEST                 = 0, 
             // Not yet supported; 0 = No
   parameter integer C_DEST_WIDTH                = 1, 
             // Not yet supported; Range: 1
   parameter [C_NUM_RANGES*C_DEST_WIDTH-1:0] C_DEST = 64'h0,
             // Not yet supported
   parameter integer C_PREFIX_WIDTH              = 1, 
             // Derived by tcl: (SI_ADDR_WIDTH == MI_ADDR_WIDTH) ? 1 : (MI_ADDR_WIDTH - SI_ADDR_WIDTH)
   parameter [C_NUM_RANGES*C_PREFIX_WIDTH-1:0] C_PREFIX = 64'h0,
             // Not yet supported; Range per element: {C_PREFIX_WIDTH{1'b0}}
   parameter integer C_S_AXI_SUPPORTS_WRITE      = 1,
             // Enable write-related channels and write address decoder
             // 0 = No; 1 = Yes
   parameter integer C_S_AXI_SUPPORTS_READ       = 1,
             // Enable read-related channels and read address decoder
             // 0 = No; 1 = Yes
   parameter [C_NUM_RANGES-1:0] C_M_AXI_SUPPORTS_WRITE = 64'h1,
             // Include corresponding address range in write address decoder
             // Per element: 0 = No, 1 = Yes
             // Concatenation of user parameters (Dddd_READ_WRITE_MODE != "READ_ONLY")
   parameter [C_NUM_RANGES-1:0] C_M_AXI_SUPPORTS_READ  = 64'h1
             // Include corresponding address range in read address decoder
             // Per element: 0 = No, 1 = Yes
             // Concatenation of user parameters (Dddd_READ_WRITE_MODE != "WRITE_ONLY")
   )
  (
   // System Signals
   input wire aclk,
   input wire aresetn,

   // Slave Interface
   input  wire [C_AXI_ID_WIDTH-1:0]                    s_axi_awid,
   input  wire [C_S_AXI_ADDR_WIDTH-1:0]                s_axi_awaddr,
   input  wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   s_axi_awlen,
   input  wire [3-1:0]                                 s_axi_awsize,
   input  wire [2-1:0]                                 s_axi_awburst,
   input  wire [((C_AXI_PROTOCOL == 1) ? 2 : 1)-1:0]   s_axi_awlock,
   input  wire [4-1:0]                                 s_axi_awcache,
   input  wire [3-1:0]                                 s_axi_awprot,
   input  wire [4-1:0]                                 s_axi_awqos,
   input  wire [C_AXI_AWUSER_WIDTH-1:0]                s_axi_awuser,
   input  wire                                         s_axi_awvalid,
   output wire                                         s_axi_awready,
   input  wire [C_AXI_ID_WIDTH-1:0]                    s_axi_wid,
   input  wire [C_AXI_DATA_WIDTH-1:0]                  s_axi_wdata,
   input  wire [C_AXI_DATA_WIDTH/8-1:0]                s_axi_wstrb,
   input  wire                                         s_axi_wlast,
   input  wire [C_AXI_WUSER_WIDTH-1:0]                 s_axi_wuser,
   input  wire                                         s_axi_wvalid,
   output wire                                         s_axi_wready,
   output wire [C_AXI_ID_WIDTH-1:0]                    s_axi_bid,
   output wire [2-1:0]                                 s_axi_bresp,
   output wire [C_AXI_BUSER_WIDTH-1:0]                 s_axi_buser,
   output wire                                         s_axi_bvalid,
   input  wire                                         s_axi_bready,
   input  wire [C_AXI_ID_WIDTH-1:0]                    s_axi_arid,
   input  wire [C_S_AXI_ADDR_WIDTH-1:0]                s_axi_araddr,
   input  wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   s_axi_arlen,
   input  wire [3-1:0]                                 s_axi_arsize,
   input  wire [2-1:0]                                 s_axi_arburst,
   input  wire [((C_AXI_PROTOCOL == 1) ? 2 : 1)-1:0]   s_axi_arlock,
   input  wire [4-1:0]                                 s_axi_arcache,
   input  wire [3-1:0]                                 s_axi_arprot,
   input  wire [4-1:0]                                 s_axi_arqos,
   input  wire [C_AXI_ARUSER_WIDTH-1:0]                s_axi_aruser,
   input  wire                                         s_axi_arvalid,
   output wire                                         s_axi_arready,
   output wire [C_AXI_ID_WIDTH-1:0]                    s_axi_rid,
   output wire [C_AXI_DATA_WIDTH-1:0]                  s_axi_rdata,
   output wire [2-1:0]                                 s_axi_rresp,
   output wire                                         s_axi_rlast,
   output wire [C_AXI_RUSER_WIDTH-1:0]                 s_axi_ruser,
   output wire                                         s_axi_rvalid,
   input  wire                                         s_axi_rready,
   
   // Master Interface
   output wire [C_AXI_ID_WIDTH-1:0]                    m_axi_awid,
   output wire [C_M_AXI_ADDR_WIDTH-1:0]                m_axi_awaddr,
   output wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   m_axi_awlen,
   output wire [3-1:0]                                 m_axi_awsize,
   output wire [2-1:0]                                 m_axi_awburst,
   output wire [((C_AXI_PROTOCOL == 1) ? 2 : 1)-1:0]   m_axi_awlock,
   output wire [4-1:0]                                 m_axi_awcache,
   output wire [3-1:0]                                 m_axi_awprot,
   output wire [4-1:0]                                 m_axi_awqos,
   output wire [C_AXI_AWUSER_WIDTH-1:0]                m_axi_awuser,
   output wire                                         m_axi_awvalid,
   input  wire                                         m_axi_awready,
   output wire [C_AXI_ID_WIDTH-1:0]                    m_axi_wid,
   output wire [C_AXI_DATA_WIDTH-1:0]                  m_axi_wdata,
   output wire [C_AXI_DATA_WIDTH/8-1:0]                m_axi_wstrb,
   output wire                                         m_axi_wlast,
   output wire [C_AXI_WUSER_WIDTH-1:0]                 m_axi_wuser,
   output wire                                         m_axi_wvalid,
   input  wire                                         m_axi_wready,
   input  wire [C_AXI_ID_WIDTH-1:0]                    m_axi_bid,
   input  wire [2-1:0]                                 m_axi_bresp,
   input  wire [C_AXI_BUSER_WIDTH-1:0]                 m_axi_buser,
   input  wire                                         m_axi_bvalid,
   output wire                                         m_axi_bready,
   output wire [C_AXI_ID_WIDTH-1:0]                    m_axi_arid,
   output wire [C_M_AXI_ADDR_WIDTH-1:0]                m_axi_araddr,
   output wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   m_axi_arlen,
   output wire [3-1:0]                                 m_axi_arsize,
   output wire [2-1:0]                                 m_axi_arburst,
   output wire [((C_AXI_PROTOCOL == 1) ? 2 : 1)-1:0]   m_axi_arlock,
   output wire [4-1:0]                                 m_axi_arcache,
   output wire [3-1:0]                                 m_axi_arprot,
   output wire [4-1:0]                                 m_axi_arqos,
   output wire [C_AXI_ARUSER_WIDTH-1:0]                m_axi_aruser,
   output wire                                         m_axi_arvalid,
   input  wire                                         m_axi_arready,
   input  wire [C_AXI_ID_WIDTH-1:0]                    m_axi_rid,
   input  wire [C_AXI_DATA_WIDTH-1:0]                  m_axi_rdata,
   input  wire [2-1:0]                                 m_axi_rresp,
   input  wire                                         m_axi_rlast,
   input  wire [C_AXI_RUSER_WIDTH-1:0]                 m_axi_ruser,
   input  wire                                         m_axi_rvalid,
   output wire                                         m_axi_rready
  );

  localparam integer  P_AXI4 = 0;
  localparam integer  P_AXI3 = 1;
  localparam integer  P_AXILITE = 2;
  localparam P_DECERR = 2'b11;
  localparam integer P_NUM_RANGES_LOG = (C_NUM_RANGES>1) ? f_ceil_log2(C_NUM_RANGES) : 1;
  
  localparam [1:0] W_IDLE     = 2'b00 ;
  localparam [1:0] W_PENDING  = 2'b01 ;
  localparam [1:0] W_DECERR   = 2'b11 ;
  localparam [1:0] R_IDLE     = 2'b00 ;
  localparam [1:0] R_PENDING  = 2'b01 ;
  localparam [1:0] R_DECERR   = 2'b11 ;
  
  // Functions

  function integer f_ceil_log2  // Ceiling of log2(x)
    (
     input integer x
     );
    integer acc;
    begin
      acc=0;
      while ((2**acc) < x)
        acc = acc + 1;
      f_ceil_log2 = acc;
    end
  endfunction

  // Internal signals
  
  wire [P_NUM_RANGES_LOG-1:0]   w_range         ;
  wire                          w_match         ;
  wire                          w_match_d       ;
  reg  [2:0]                    w_state = W_IDLE         ;
  reg  [5:0]                    aw_cnt          ;
  reg  [5:0]                    w_cnt           ;
  wire                          err_awvalid     ;
  wire                          err_awready     ;
  wire                          err_wvalid      ;
  wire                          err_wready      ;
  wire                          err_bvalid      ;
  wire [C_AXI_ID_WIDTH-1:0]     err_bid         ;
  wire [1:0]                    err_bresp       ;
  wire [C_AXI_BUSER_WIDTH-1:0]  err_buser       ;
  wire                          aw_push         ;
  wire                          aw_pop          ;
  wire                          w_push          ;
  wire                          w_pop           ;
  reg                           w_mask          ;
  wire [P_NUM_RANGES_LOG-1:0]   r_range         ;
  wire                          r_match         ;
  wire                          r_match_d       ;
  reg  [2:0]                    r_state = R_IDLE         ;
  reg  [5:0]                    ar_cnt          ;
  wire                          err_arvalid     ;
  wire                          err_arready     ;
  wire [8-1:0]                  err_arlen       ;
  wire                          err_rvalid      ;
  wire                          err_rlast       ;
  wire [C_AXI_ID_WIDTH-1:0]     err_rid         ;
  wire [1:0]                    err_rresp       ;
  wire [C_AXI_RUSER_WIDTH-1:0]  err_ruser       ;
  wire [C_AXI_DATA_WIDTH-1:0]   err_rdata       ;
  wire                          ar_push         ;
  wire                          ar_pop          ;
  
  wire [C_AXI_ID_WIDTH-1:0]                    sr_axi_awid;
  wire [C_M_AXI_ADDR_WIDTH:0]                  sr_axi_awaddr;
  wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   sr_axi_awlen;
  wire [3-1:0]                                 sr_axi_awsize;
  wire [2-1:0]                                 sr_axi_awburst;
  wire [((C_AXI_PROTOCOL == 1) ? 2 : 1)-1:0]   sr_axi_awlock;
  wire [4-1:0]                                 sr_axi_awcache;
  wire [3-1:0]                                 sr_axi_awprot;
  wire [4-1:0]                                 sr_axi_awqos;
  wire [C_AXI_AWUSER_WIDTH-1:0]                sr_axi_awuser;
  wire                                         sr_axi_awvalid;
  wire                                         sr_axi_awready;
  wire [C_AXI_ID_WIDTH-1:0]                    sr_axi_wid;
  wire [C_AXI_DATA_WIDTH-1:0]                  sr_axi_wdata;
  wire [C_AXI_DATA_WIDTH/8-1:0]                sr_axi_wstrb;
  wire                                         sr_axi_wlast;
  wire [C_AXI_WUSER_WIDTH-1:0]                 sr_axi_wuser;
  wire                                         sr_axi_wvalid;
  wire                                         sr_axi_bready;
  wire [C_AXI_ID_WIDTH-1:0]                    sr_axi_arid;
  wire [C_M_AXI_ADDR_WIDTH:0]                  sr_axi_araddr;
  wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   sr_axi_arlen;
  wire [3-1:0]                                 sr_axi_arsize;
  wire [2-1:0]                                 sr_axi_arburst;
  wire [((C_AXI_PROTOCOL == 1) ? 2 : 1)-1:0]   sr_axi_arlock;
  wire [4-1:0]                                 sr_axi_arcache;
  wire [3-1:0]                                 sr_axi_arprot;
  wire [4-1:0]                                 sr_axi_arqos;
  wire [C_AXI_ARUSER_WIDTH-1:0]                sr_axi_aruser;
  wire                                         sr_axi_arvalid;
  wire                                         sr_axi_arready;
  wire                                         sr_axi_rready;
  wire [C_AXI_ID_WIDTH-1:0]                    mr_axi_awid;
  wire [C_M_AXI_ADDR_WIDTH:0]                  mr_axi_awaddr;
  wire                                         mr_axi_awvalid;
  wire                                         mr_axi_awready;
  wire                                         mr_axi_wready;
  wire [C_AXI_ID_WIDTH-1:0]                    mr_axi_bid;
  wire [2-1:0]                                 mr_axi_bresp;
  wire [C_AXI_BUSER_WIDTH-1:0]                 mr_axi_buser;
  wire                                         mr_axi_bvalid;
  wire [C_AXI_ID_WIDTH-1:0]                    mr_axi_arid;
  wire [C_M_AXI_ADDR_WIDTH:0]                  mr_axi_araddr;
  wire [((C_AXI_PROTOCOL == 1) ? 4 : 8)-1:0]   mr_axi_arlen;
  wire                                         mr_axi_arvalid;
  wire                                         mr_axi_arready;
  wire [C_AXI_ID_WIDTH-1:0]                    mr_axi_rid;
  wire [C_AXI_DATA_WIDTH-1:0]                  mr_axi_rdata;
  wire [2-1:0]                                 mr_axi_rresp;
  wire                                         mr_axi_rlast;
  wire [C_AXI_RUSER_WIDTH-1:0]                 mr_axi_ruser;
  wire                                         mr_axi_rvalid;
  
  reg [1:0] aresetn_d = 2'b00; // Reset delay shifter
  always @(posedge aclk) begin
    if (~aresetn) begin
      aresetn_d <= 2'b00;
    end else begin
      aresetn_d <= {aresetn_d[0], aresetn};
    end
  end
      
  generate
  
  if (C_S_AXI_SUPPORTS_WRITE != 0) begin : gen_write
    
    assign sr_axi_awaddr   = (w_match << C_M_AXI_ADDR_WIDTH) | ((C_M_AXI_ADDR_WIDTH > C_S_AXI_ADDR_WIDTH) ? {C_PREFIX[w_range*C_PREFIX_WIDTH +: C_PREFIX_WIDTH], s_axi_awaddr} : s_axi_awaddr);
    assign m_axi_awaddr    = mr_axi_awaddr[C_M_AXI_ADDR_WIDTH-1:0];
    assign w_match_d       = mr_axi_awaddr[C_M_AXI_ADDR_WIDTH];
    assign sr_axi_awid     = s_axi_awid;
    assign m_axi_awid      = mr_axi_awid;
    assign sr_axi_awlen    = s_axi_awlen;
    assign sr_axi_awsize   = s_axi_awsize;
    assign sr_axi_awburst  = s_axi_awburst;
    assign sr_axi_awlock   = s_axi_awlock;
    assign sr_axi_awcache  = s_axi_awcache;
    assign sr_axi_awprot   = s_axi_awprot;
    assign sr_axi_awqos    = s_axi_awqos;
    assign sr_axi_awuser   = s_axi_awuser;
    assign sr_axi_wid      = s_axi_wid;
    assign sr_axi_wdata    = s_axi_wdata;
    assign sr_axi_wstrb    = s_axi_wstrb;
    assign sr_axi_wlast    = s_axi_wlast;
    assign sr_axi_wuser    = s_axi_wuser;

    axi_mmu_v2_1_17_addr_decoder #
      (
        .C_FAMILY          (C_FAMILY),
        .C_NUM_RANGES      (C_NUM_RANGES),
        .C_NUM_RANGES_LOG  (P_NUM_RANGES_LOG),
        .C_ADDR_WIDTH      (C_S_AXI_ADDR_WIDTH),
        .C_BASE_ADDR       (C_BASE_ADDR),
        .C_RANGE_SIZE      (C_RANGE_SIZE),
        .C_RANGE_QUAL      (C_M_AXI_SUPPORTS_WRITE)
      ) 
      write_decoder
      (
        .addr             (s_axi_awaddr),        
        .range_enc        (w_range),  
        .match            (w_match)     
      );
  
    always @(posedge aclk) begin
      if (~aresetn_d[1]) begin
        w_state <= W_IDLE;
      end else begin
        case (w_state)
          W_DECERR: begin
            if (err_bvalid & s_axi_bready) begin
              if (mr_axi_awvalid & ~w_match_d) begin
                w_state <= W_PENDING;
              end else begin
                w_state <= W_IDLE;
              end
            end
          end
            
          W_PENDING: begin
            if ((aw_cnt == 0) && (w_cnt == 0)) begin
              w_state <= W_DECERR;
            end
          end
          
          default: begin  // W_IDLE
            if (mr_axi_awvalid & ~w_match_d) begin
              w_state <= W_PENDING;
            end
          end
        endcase
      end
    end
        
    always @(posedge aclk) begin
      if (~aresetn_d[1]) begin
        aw_cnt <= 'b0;
        w_cnt <= 'b0;
        w_mask <= 1'b0;
      end else begin
        if (aw_push & ~aw_pop) begin
          aw_cnt <= aw_cnt + 1;
        end else if (~aw_push & aw_pop) begin
          aw_cnt <= aw_cnt - 1;
        end
        
        if (w_push & ~w_pop) begin
          w_cnt <= w_cnt + 1;
        end else if (~w_push & w_pop) begin
          w_cnt <= w_cnt - 1;
        end
        
        if ((sr_axi_awready & w_match & ~aw_cnt[5]) || ((w_state == W_PENDING) && (aw_cnt == 0) && (w_cnt == 0))) begin
          w_mask <= 1'b0;
        end else if ((sr_axi_awvalid & w_match) | (sr_axi_awvalid & ~w_match)) begin
          w_mask <= 1'b1;
        end
      end
    end
    
    assign aw_push = sr_axi_awvalid & sr_axi_awready & w_match;
    assign aw_pop = m_axi_bvalid & s_axi_bready & (aw_cnt != 0);
    assign w_push = sr_axi_awvalid & w_match & ~w_mask;
    assign w_pop = sr_axi_wvalid & m_axi_wready & ((C_AXI_PROTOCOL == P_AXILITE) ? 1'b1 : s_axi_wlast);
    
    assign sr_axi_awvalid = s_axi_awvalid & ~aw_cnt[5];
    assign m_axi_awvalid = (mr_axi_awvalid & w_match_d) && (w_state != W_PENDING) && (w_state != W_DECERR);
    assign mr_axi_awready = (w_state == W_PENDING) ? 1'b0 : (w_state == W_DECERR) ? (err_awready & ~w_match_d) : (m_axi_awready & w_match_d);
    assign s_axi_awready = sr_axi_awready & ~aw_cnt[5];
    assign err_awvalid = (w_state == W_DECERR) & mr_axi_awvalid & ~w_match_d;
    assign sr_axi_wvalid = (w_state != W_DECERR) & (w_cnt != 0) & s_axi_wvalid;
    assign mr_axi_wready = (w_state == W_DECERR) ? err_wready : ((w_cnt != 0) & m_axi_wready);
    assign err_wvalid = (w_state == W_DECERR) & s_axi_wvalid;
    assign mr_axi_bvalid = (w_state == W_DECERR) ? err_bvalid : m_axi_bvalid;
    assign sr_axi_bready = s_axi_bready;
    assign mr_axi_bid   = (w_state == W_DECERR) ? err_bid   : m_axi_bid;
    assign mr_axi_bresp = (w_state == W_DECERR) ? err_bresp : m_axi_bresp;
    assign mr_axi_buser = (w_state == W_DECERR) ? err_buser : m_axi_buser;
    
  end else begin : gen_w_tieoff
    
    assign sr_axi_awvalid  = 1'b0;
    assign m_axi_awvalid   = 1'b0;
    assign mr_axi_awready  = 1'b0;
    assign s_axi_awready   = 1'b0;
    assign sr_axi_awid     = 'b0;
    assign m_axi_awid      = 'b0;
    assign sr_axi_awaddr   = 'b0;
    assign m_axi_awaddr    = 'b0;
    assign sr_axi_awlen    = 'b0;
    assign sr_axi_awsize   = 'b0;
    assign sr_axi_awburst  = 'b0;
    assign sr_axi_awlock   = 'b0;
    assign sr_axi_awcache  = 'b0;
    assign sr_axi_awprot   = 'b0;
    assign sr_axi_awqos    = 'b0;
    assign sr_axi_awuser   = 'b0;
    assign sr_axi_wid      = 'b0;
    assign sr_axi_wdata    = 'b0;
    assign sr_axi_wstrb    = 'b0;
    assign sr_axi_wlast    = 1'b0;
    assign sr_axi_wuser    = 'b0;
    assign sr_axi_wvalid   = 1'b0;
    assign mr_axi_wready   = 1'b0;
    assign mr_axi_bid      = 'b0;
    assign mr_axi_bresp    = 'b0;
    assign mr_axi_buser    = 'b0;
    assign mr_axi_bvalid   = 1'b0;
    assign sr_axi_bready   = 1'b0;
    assign err_awvalid     = 1'b0;
    assign err_wvalid      = 1'b0;
    
  end  // Write
  
  if (C_S_AXI_SUPPORTS_READ != 0) begin : gen_read
    
    assign sr_axi_araddr   = (r_match << C_M_AXI_ADDR_WIDTH) | ((C_M_AXI_ADDR_WIDTH > C_S_AXI_ADDR_WIDTH) ? {C_PREFIX[r_range*C_PREFIX_WIDTH +: C_PREFIX_WIDTH], s_axi_araddr} : s_axi_araddr);
    assign m_axi_araddr    = mr_axi_araddr[C_M_AXI_ADDR_WIDTH-1:0];
    assign r_match_d       = mr_axi_araddr[C_M_AXI_ADDR_WIDTH];
    assign sr_axi_arid     = s_axi_arid;
    assign m_axi_arid      = mr_axi_arid;
    assign sr_axi_arlen    = s_axi_arlen;
    assign m_axi_arlen     = mr_axi_arlen;
    assign sr_axi_arsize   = s_axi_arsize;
    assign sr_axi_arburst  = s_axi_arburst;
    assign sr_axi_arlock   = s_axi_arlock;
    assign sr_axi_arcache  = s_axi_arcache;
    assign sr_axi_arprot   = s_axi_arprot;
    assign sr_axi_arqos    = s_axi_arqos;
    assign sr_axi_aruser   = s_axi_aruser;

   axi_mmu_v2_1_17_addr_decoder #
      (
        .C_FAMILY          (C_FAMILY),
        .C_NUM_RANGES      (C_NUM_RANGES),
        .C_NUM_RANGES_LOG  (P_NUM_RANGES_LOG),
        .C_ADDR_WIDTH      (C_S_AXI_ADDR_WIDTH),
        .C_BASE_ADDR       (C_BASE_ADDR),
        .C_RANGE_SIZE      (C_RANGE_SIZE),
        .C_RANGE_QUAL      (C_M_AXI_SUPPORTS_READ)
      ) 
      read_decoder
      (
        .addr             (s_axi_araddr),        
        .range_enc        (r_range),  
        .match            (r_match)     
      );
      
    always @(posedge aclk) begin
      if (~aresetn_d[1]) begin
        r_state <= R_IDLE;
      end else begin
        case (r_state)
          R_DECERR: begin
            if (err_rvalid & err_rlast & s_axi_rready) begin
              if (mr_axi_arvalid & ~r_match_d) begin
                r_state <= R_DECERR;
              end else begin
                r_state <= R_IDLE;
              end
            end
          end
            
          R_PENDING: begin
            if (ar_cnt == 0) begin
              r_state <= R_DECERR;
            end
          end
          
          default: begin  // R_IDLE
            if (mr_axi_arvalid & ~r_match_d) begin
              r_state <= R_PENDING;
            end
          end
        endcase
      end
    end
        
    always @(posedge aclk) begin
      if (~aresetn_d[1]) begin
        ar_cnt <= 0;
      end else begin
        if (ar_push & ~ar_pop) begin
          ar_cnt <= ar_cnt + 1;
        end else if (~ar_push & ar_pop) begin
          ar_cnt <= ar_cnt - 1;
        end
      end
    end
    
    assign ar_push = sr_axi_arvalid & sr_axi_arready & r_match;
    assign ar_pop = m_axi_rvalid & s_axi_rready & ((C_AXI_PROTOCOL == P_AXILITE) ? 1'b1 : m_axi_rlast) & (ar_cnt != 0);
    
    assign sr_axi_arvalid = s_axi_arvalid & ~ar_cnt[5];
    assign m_axi_arvalid = (mr_axi_arvalid & r_match_d) && (r_state != R_PENDING) && (r_state != R_DECERR);
    assign mr_axi_arready = (r_state == R_PENDING) ? 1'b0 : (r_state == R_DECERR) ? (err_arready & ~r_match_d) : (m_axi_arready & r_match_d);
    assign s_axi_arready = sr_axi_arready & ~ar_cnt[5];
    assign err_arvalid = (r_state == R_DECERR) & mr_axi_arvalid & ~r_match_d;
    assign mr_axi_rvalid = (r_state == R_DECERR) ? err_rvalid : m_axi_rvalid;
    assign sr_axi_rready = s_axi_rready;
    assign mr_axi_rlast = (r_state == R_DECERR) ? err_rlast : m_axi_rlast;
    assign mr_axi_rid   = (r_state == R_DECERR) ? err_rid   : m_axi_rid;
    assign mr_axi_rdata = (r_state == R_DECERR) ? err_rdata : m_axi_rdata;
    assign mr_axi_rresp = (r_state == R_DECERR) ? err_rresp : m_axi_rresp;
    assign mr_axi_ruser = (r_state == R_DECERR) ? err_ruser : m_axi_ruser;
    assign err_arlen = mr_axi_arlen;
          
  end else begin : gen_r_tieoff
    
    assign sr_axi_arvalid  = 1'b0;
    assign m_axi_arvalid   = 1'b0;
    assign mr_axi_arready  = 1'b0;
    assign s_axi_arready   = 1'b0;
    assign sr_axi_arid     = 'b0;
    assign m_axi_arid      = 'b0;
    assign sr_axi_araddr   = 'b0;
    assign m_axi_araddr    = 'b0;
    assign sr_axi_arlen    = 'b0;
    assign m_axi_arlen     = 'b0;
    assign sr_axi_arsize   = 'b0;
    assign sr_axi_arburst  = 'b0;
    assign sr_axi_arlock   = 'b0;
    assign sr_axi_arcache  = 'b0;
    assign sr_axi_arprot   = 'b0;
    assign sr_axi_arqos    = 'b0;
    assign sr_axi_aruser   = 'b0;
    assign mr_axi_rid      = 'b0;
    assign mr_axi_rdata    = 'b0;
    assign mr_axi_rresp    = 'b0;
    assign mr_axi_rlast    = 1'b0;
    assign mr_axi_ruser    = 'b0;
    assign mr_axi_rvalid   = 1'b0;
    assign sr_axi_rready   = 1'b0;
    assign err_arvalid     = 1'b0;
    assign err_arlen       = 1'b0;
    
  end
        
  axi_mmu_v2_1_17_decerr_slave #
    (
     .C_AXI_ID_WIDTH                 (C_AXI_ID_WIDTH),
     .C_AXI_DATA_WIDTH               (C_AXI_DATA_WIDTH),
     .C_AXI_RUSER_WIDTH              (C_AXI_RUSER_WIDTH),
     .C_AXI_BUSER_WIDTH              (C_AXI_BUSER_WIDTH),
     .C_AXI_PROTOCOL                 (C_AXI_PROTOCOL),
     .C_AXI_SUPPORTS_WRITE           (C_S_AXI_SUPPORTS_WRITE),
     .C_AXI_SUPPORTS_READ            (C_S_AXI_SUPPORTS_READ),
     .C_RESP                         (P_DECERR) 
    )
    decerr_slave_inst
      (
       .aclk (aclk),
       .aresetn (aresetn_d[1]),
       .s_axi_awid (mr_axi_awid),
       .s_axi_awvalid (err_awvalid),
       .s_axi_awready (err_awready),
       .s_axi_wlast (s_axi_wlast),
       .s_axi_wvalid (err_wvalid),
       .s_axi_wready (err_wready),
       .s_axi_bid (err_bid),
       .s_axi_bresp (err_bresp),
       .s_axi_buser (err_buser),
       .s_axi_bvalid (err_bvalid),
       .s_axi_bready (s_axi_bready),
       .s_axi_arid (mr_axi_arid),
       .s_axi_arlen (err_arlen),
       .s_axi_arvalid (err_arvalid),
       .s_axi_arready (err_arready),
       .s_axi_rid (err_rid),
       .s_axi_rdata (err_rdata),
       .s_axi_rresp (err_rresp),
       .s_axi_ruser (err_ruser),
       .s_axi_rlast (err_rlast),
       .s_axi_rvalid (err_rvalid),
       .s_axi_rready (s_axi_rready)
     );
     
  axi_register_slice_v2_1_19_axi_register_slice #(
    .C_FAMILY(C_FAMILY),
    .C_AXI_PROTOCOL(C_AXI_PROTOCOL),
    .C_AXI_ID_WIDTH(C_AXI_ID_WIDTH),
    .C_AXI_ADDR_WIDTH(C_M_AXI_ADDR_WIDTH+1),
    .C_AXI_DATA_WIDTH(C_AXI_DATA_WIDTH),
    .C_AXI_SUPPORTS_USER_SIGNALS(C_AXI_SUPPORTS_USER_SIGNALS),
    .C_AXI_AWUSER_WIDTH(C_AXI_AWUSER_WIDTH),
    .C_AXI_ARUSER_WIDTH(C_AXI_ARUSER_WIDTH),
    .C_AXI_WUSER_WIDTH(C_AXI_WUSER_WIDTH),
    .C_AXI_RUSER_WIDTH(C_AXI_RUSER_WIDTH),
    .C_AXI_BUSER_WIDTH(C_AXI_BUSER_WIDTH),
    .C_REG_CONFIG_AW(C_S_AXI_SUPPORTS_WRITE ? 7 : 0),
    .C_REG_CONFIG_W(0),
    .C_REG_CONFIG_B(0),
    .C_REG_CONFIG_AR(C_S_AXI_SUPPORTS_READ ? 7 : 0),
    .C_REG_CONFIG_R(0)
  ) register_slice_inst (
    .aclk             (aclk),
    .aresetn          (aresetn),
    .s_axi_awid       (sr_axi_awid),
    .s_axi_awaddr     (sr_axi_awaddr),
    .s_axi_awlen      (sr_axi_awlen),
    .s_axi_awsize     (sr_axi_awsize),
    .s_axi_awburst    (sr_axi_awburst),
    .s_axi_awlock     (sr_axi_awlock),
    .s_axi_awcache    (sr_axi_awcache),
    .s_axi_awprot     (sr_axi_awprot),
    .s_axi_awregion   (4'h0),
    .s_axi_awqos      (sr_axi_awqos),
    .s_axi_awuser     (sr_axi_awuser),
    .s_axi_awvalid    (sr_axi_awvalid),
    .s_axi_awready    (sr_axi_awready),
    .s_axi_wid        (sr_axi_wid),
    .s_axi_wdata      (sr_axi_wdata),
    .s_axi_wstrb      (sr_axi_wstrb),
    .s_axi_wlast      (sr_axi_wlast),
    .s_axi_wuser      (sr_axi_wuser),
    .s_axi_wvalid     (sr_axi_wvalid),
    .s_axi_wready     (s_axi_wready),
    .s_axi_bid        (s_axi_bid),
    .s_axi_bresp      (s_axi_bresp),
    .s_axi_buser      (s_axi_buser),
    .s_axi_bvalid     (s_axi_bvalid),
    .s_axi_bready     (sr_axi_bready),
    .s_axi_arid       (sr_axi_arid),
    .s_axi_araddr     (sr_axi_araddr),
    .s_axi_arlen      (sr_axi_arlen),
    .s_axi_arsize     (sr_axi_arsize),
    .s_axi_arburst    (sr_axi_arburst),
    .s_axi_arlock     (sr_axi_arlock),
    .s_axi_arcache    (sr_axi_arcache),
    .s_axi_arprot     (sr_axi_arprot),
    .s_axi_arregion   (4'h0),
    .s_axi_arqos      (sr_axi_arqos),
    .s_axi_aruser     (sr_axi_aruser),
    .s_axi_arvalid    (sr_axi_arvalid),
    .s_axi_arready    (sr_axi_arready),
    .s_axi_rid        (s_axi_rid),
    .s_axi_rdata      (s_axi_rdata),
    .s_axi_rresp      (s_axi_rresp),
    .s_axi_rlast      (s_axi_rlast),
    .s_axi_ruser      (s_axi_ruser),
    .s_axi_rvalid     (s_axi_rvalid),
    .s_axi_rready     (sr_axi_rready),
    .m_axi_awid       (mr_axi_awid),
    .m_axi_awaddr     (mr_axi_awaddr),
    .m_axi_awlen      (m_axi_awlen),
    .m_axi_awsize     (m_axi_awsize),
    .m_axi_awburst    (m_axi_awburst),
    .m_axi_awlock     (m_axi_awlock),
    .m_axi_awcache    (m_axi_awcache),
    .m_axi_awprot     (m_axi_awprot),
    .m_axi_awregion   (),
    .m_axi_awqos      (m_axi_awqos),
    .m_axi_awuser     (m_axi_awuser),
    .m_axi_awvalid    (mr_axi_awvalid),
    .m_axi_awready    (mr_axi_awready),
    .m_axi_wid        (m_axi_wid),
    .m_axi_wdata      (m_axi_wdata),
    .m_axi_wstrb      (m_axi_wstrb),
    .m_axi_wlast      (m_axi_wlast),
    .m_axi_wuser      (m_axi_wuser),
    .m_axi_wvalid     (m_axi_wvalid),
    .m_axi_wready     (mr_axi_wready),
    .m_axi_bid        (mr_axi_bid),
    .m_axi_bresp      (mr_axi_bresp),
    .m_axi_buser      (mr_axi_buser),
    .m_axi_bvalid     (mr_axi_bvalid),
    .m_axi_bready     (m_axi_bready),
    .m_axi_arid       (mr_axi_arid),
    .m_axi_araddr     (mr_axi_araddr),
    .m_axi_arlen      (mr_axi_arlen),
    .m_axi_arsize     (m_axi_arsize),
    .m_axi_arburst    (m_axi_arburst),
    .m_axi_arlock     (m_axi_arlock),
    .m_axi_arcache    (m_axi_arcache),
    .m_axi_arprot     (m_axi_arprot),
    .m_axi_arregion   (),
    .m_axi_arqos      (m_axi_arqos),
    .m_axi_aruser     (m_axi_aruser),
    .m_axi_arvalid    (mr_axi_arvalid),
    .m_axi_arready    (mr_axi_arready),
    .m_axi_rid        (mr_axi_rid),
    .m_axi_rdata      (mr_axi_rdata),
    .m_axi_rresp      (mr_axi_rresp),
    .m_axi_rlast      (mr_axi_rlast),
    .m_axi_ruser      (mr_axi_ruser),
    .m_axi_rvalid     (mr_axi_rvalid),
    .m_axi_rready     (m_axi_rready)
  );
  
  endgenerate

endmodule // axi_mmu


