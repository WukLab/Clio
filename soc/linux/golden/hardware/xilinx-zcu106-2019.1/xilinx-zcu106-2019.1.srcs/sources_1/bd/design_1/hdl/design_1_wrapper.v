//Copyright 1986-2019 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2019.1 (lin64) Build 2552052 Fri May 24 14:47:09 MDT 2019
//Date        : Sat May 25 00:41:56 2019
//Host        : xcosswbld06 running 64-bit Red Hat Enterprise Linux Workstation release 7.2 (Maipo)
//Command     : generate_target design_1_wrapper.bd
//Design      : design_1_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module design_1_wrapper
   (C0_DDR4_act_n,
    C0_DDR4_adr,
    C0_DDR4_ba,
    C0_DDR4_bg,
    C0_DDR4_ck_c,
    C0_DDR4_ck_t,
    C0_DDR4_cke,
    C0_DDR4_cs_n,
    C0_DDR4_dm_n,
    C0_DDR4_dq,
    C0_DDR4_dqs_c,
    C0_DDR4_dqs_t,
    C0_DDR4_odt,
    C0_DDR4_reset_n,
    mig_sys_clk_n,
    mig_sys_clk_p,
    si570_user_clk_n,
    si570_user_clk_p);
  output [0:0]C0_DDR4_act_n;
  output [16:0]C0_DDR4_adr;
  output [1:0]C0_DDR4_ba;
  output [0:0]C0_DDR4_bg;
  output [0:0]C0_DDR4_ck_c;
  output [0:0]C0_DDR4_ck_t;
  output [0:0]C0_DDR4_cke;
  output [0:0]C0_DDR4_cs_n;
  inout [7:0]C0_DDR4_dm_n;
  inout [63:0]C0_DDR4_dq;
  inout [7:0]C0_DDR4_dqs_c;
  inout [7:0]C0_DDR4_dqs_t;
  output [0:0]C0_DDR4_odt;
  output [0:0]C0_DDR4_reset_n;
  input [0:0]mig_sys_clk_n;
  input [0:0]mig_sys_clk_p;
  input si570_user_clk_n;
  input si570_user_clk_p;

  wire [0:0]C0_DDR4_act_n;
  wire [16:0]C0_DDR4_adr;
  wire [1:0]C0_DDR4_ba;
  wire [0:0]C0_DDR4_bg;
  wire [0:0]C0_DDR4_ck_c;
  wire [0:0]C0_DDR4_ck_t;
  wire [0:0]C0_DDR4_cke;
  wire [0:0]C0_DDR4_cs_n;
  wire [7:0]C0_DDR4_dm_n;
  wire [63:0]C0_DDR4_dq;
  wire [7:0]C0_DDR4_dqs_c;
  wire [7:0]C0_DDR4_dqs_t;
  wire [0:0]C0_DDR4_odt;
  wire [0:0]C0_DDR4_reset_n;
  wire [0:0]mig_sys_clk_n;
  wire [0:0]mig_sys_clk_p;
  wire si570_user_clk_n;
  wire si570_user_clk_p;

  design_1 design_1_i
       (.C0_DDR4_act_n(C0_DDR4_act_n),
        .C0_DDR4_adr(C0_DDR4_adr),
        .C0_DDR4_ba(C0_DDR4_ba),
        .C0_DDR4_bg(C0_DDR4_bg),
        .C0_DDR4_ck_c(C0_DDR4_ck_c),
        .C0_DDR4_ck_t(C0_DDR4_ck_t),
        .C0_DDR4_cke(C0_DDR4_cke),
        .C0_DDR4_cs_n(C0_DDR4_cs_n),
        .C0_DDR4_dm_n(C0_DDR4_dm_n),
        .C0_DDR4_dq(C0_DDR4_dq),
        .C0_DDR4_dqs_c(C0_DDR4_dqs_c),
        .C0_DDR4_dqs_t(C0_DDR4_dqs_t),
        .C0_DDR4_odt(C0_DDR4_odt),
        .C0_DDR4_reset_n(C0_DDR4_reset_n),
        .mig_sys_clk_n(mig_sys_clk_n),
        .mig_sys_clk_p(mig_sys_clk_p),
        .si570_user_clk_n(si570_user_clk_n),
        .si570_user_clk_p(si570_user_clk_p));
endmodule
