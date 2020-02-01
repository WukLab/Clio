-- Copyright 1986-2019 Xilinx, Inc. All Rights Reserved.
-- --------------------------------------------------------------------------------
-- Tool Version: Vivado v.2019.1 (lin64) Build 2552052 Fri May 24 14:47:09 MDT 2019
-- Date        : Sat May 25 00:55:36 2019
-- Host        : xcosswbld06 running 64-bit Red Hat Enterprise Linux Workstation release 7.2 (Maipo)
-- Command     : write_vhdl -force -mode synth_stub
--               /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_vcu_ddr4_controller_0_0/design_1_vcu_ddr4_controller_0_0_stub.vhdl
-- Design      : design_1_vcu_ddr4_controller_0_0
-- Purpose     : Stub declaration of top-level module interface
-- Device      : xczu7ev-ffvc1156-2-e
-- --------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity design_1_vcu_ddr4_controller_0_0 is
  Port ( 
    S_Axi_Clk : in STD_LOGIC;
    S_Axi_Rst : in STD_LOGIC;
    pl_barco_slot_wstrb0 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wstrb1 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wstrb2 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wstrb3 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wstrb4 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_araddr0 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_arburst0 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_arid0 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_arlen0 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_arready0 : out STD_LOGIC;
    pl_barco_slot_arsize0 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_arvalid0 : in STD_LOGIC;
    pl_barco_slot_awaddr0 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_awburst0 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_awid0 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_awlen0 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_awready0 : out STD_LOGIC;
    pl_barco_slot_awsize0 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_awvalid0 : in STD_LOGIC;
    pl_barco_slot_bready0 : in STD_LOGIC;
    barco_pl_slot_bvalid0 : out STD_LOGIC;
    barco_pl_slot_bid0 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rdata0 : out STD_LOGIC_VECTOR ( 127 downto 0 );
    barco_pl_slot_rid0 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rlast0 : out STD_LOGIC;
    pl_barco_slot_rready0 : in STD_LOGIC;
    barco_pl_slot_rvalid0 : out STD_LOGIC;
    pl_barco_slot_wid0 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wdata0 : in STD_LOGIC_VECTOR ( 127 downto 0 );
    pl_barco_slot_wlast0 : in STD_LOGIC;
    barco_pl_slot_bresp0 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_rresp0 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_wready0 : out STD_LOGIC;
    pl_barco_slot_wvalid0 : in STD_LOGIC;
    pl_barco_slot_araddr1 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_arburst1 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_arid1 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_arlen1 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_arready1 : out STD_LOGIC;
    pl_barco_slot_arsize1 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_arvalid1 : in STD_LOGIC;
    pl_barco_slot_awaddr1 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_awburst1 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_awid1 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_awlen1 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_awready1 : out STD_LOGIC;
    pl_barco_slot_awsize1 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_awvalid1 : in STD_LOGIC;
    pl_barco_slot_bready1 : in STD_LOGIC;
    barco_pl_slot_bvalid1 : out STD_LOGIC;
    barco_pl_slot_bid1 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rdata1 : out STD_LOGIC_VECTOR ( 127 downto 0 );
    barco_pl_slot_rid1 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rlast1 : out STD_LOGIC;
    pl_barco_slot_rready1 : in STD_LOGIC;
    barco_pl_slot_rvalid1 : out STD_LOGIC;
    pl_barco_slot_wid1 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wdata1 : in STD_LOGIC_VECTOR ( 127 downto 0 );
    pl_barco_slot_wlast1 : in STD_LOGIC;
    barco_pl_slot_bresp1 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_rresp1 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_wready1 : out STD_LOGIC;
    pl_barco_slot_wvalid1 : in STD_LOGIC;
    pl_barco_slot_araddr2 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_arburst2 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_arid2 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_arlen2 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_arready2 : out STD_LOGIC;
    pl_barco_slot_arsize2 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_arvalid2 : in STD_LOGIC;
    pl_barco_slot_awaddr2 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_awburst2 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_awid2 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_awlen2 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_awready2 : out STD_LOGIC;
    pl_barco_slot_awsize2 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_awvalid2 : in STD_LOGIC;
    pl_barco_slot_bready2 : in STD_LOGIC;
    barco_pl_slot_bvalid2 : out STD_LOGIC;
    barco_pl_slot_bid2 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rdata2 : out STD_LOGIC_VECTOR ( 127 downto 0 );
    barco_pl_slot_rid2 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rlast2 : out STD_LOGIC;
    pl_barco_slot_rready2 : in STD_LOGIC;
    barco_pl_slot_rvalid2 : out STD_LOGIC;
    pl_barco_slot_wid2 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wdata2 : in STD_LOGIC_VECTOR ( 127 downto 0 );
    pl_barco_slot_wlast2 : in STD_LOGIC;
    barco_pl_slot_bresp2 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_rresp2 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_wready2 : out STD_LOGIC;
    pl_barco_slot_wvalid2 : in STD_LOGIC;
    pl_barco_slot_araddr3 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_arburst3 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_arid3 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_arlen3 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_arready3 : out STD_LOGIC;
    pl_barco_slot_arsize3 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_arvalid3 : in STD_LOGIC;
    pl_barco_slot_awaddr3 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_awburst3 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_awid3 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_awlen3 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_awready3 : out STD_LOGIC;
    pl_barco_slot_awsize3 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_awvalid3 : in STD_LOGIC;
    pl_barco_slot_bready3 : in STD_LOGIC;
    barco_pl_slot_bvalid3 : out STD_LOGIC;
    barco_pl_slot_bid3 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rdata3 : out STD_LOGIC_VECTOR ( 127 downto 0 );
    barco_pl_slot_rid3 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rlast3 : out STD_LOGIC;
    pl_barco_slot_rready3 : in STD_LOGIC;
    barco_pl_slot_rvalid3 : out STD_LOGIC;
    pl_barco_slot_wid3 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wdata3 : in STD_LOGIC_VECTOR ( 127 downto 0 );
    pl_barco_slot_wlast3 : in STD_LOGIC;
    barco_pl_slot_bresp3 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_rresp3 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_wready3 : out STD_LOGIC;
    pl_barco_slot_wvalid3 : in STD_LOGIC;
    pl_barco_slot_araddr4 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_arburst4 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_arid4 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_arlen4 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_arready4 : out STD_LOGIC;
    pl_barco_slot_arsize4 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_arvalid4 : in STD_LOGIC;
    pl_barco_slot_awaddr4 : in STD_LOGIC_VECTOR ( 31 downto 0 );
    pl_barco_slot_awburst4 : in STD_LOGIC_VECTOR ( 1 downto 0 );
    pl_barco_slot_awid4 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_awlen4 : in STD_LOGIC_VECTOR ( 7 downto 0 );
    barco_pl_slot_awready4 : out STD_LOGIC;
    pl_barco_slot_awsize4 : in STD_LOGIC_VECTOR ( 2 downto 0 );
    pl_barco_slot_awvalid4 : in STD_LOGIC;
    pl_barco_slot_bready4 : in STD_LOGIC;
    barco_pl_slot_bvalid4 : out STD_LOGIC;
    barco_pl_slot_bid4 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rdata4 : out STD_LOGIC_VECTOR ( 127 downto 0 );
    barco_pl_slot_rid4 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    barco_pl_slot_rlast4 : out STD_LOGIC;
    pl_barco_slot_rready4 : in STD_LOGIC;
    barco_pl_slot_rvalid4 : out STD_LOGIC;
    pl_barco_slot_wid4 : in STD_LOGIC_VECTOR ( 15 downto 0 );
    pl_barco_slot_wdata4 : in STD_LOGIC_VECTOR ( 127 downto 0 );
    pl_barco_slot_wlast4 : in STD_LOGIC;
    barco_pl_slot_bresp4 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_rresp4 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    barco_pl_slot_wready4 : out STD_LOGIC;
    pl_barco_slot_wvalid4 : in STD_LOGIC;
    c0_ddr4_act_n : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_adr : out STD_LOGIC_VECTOR ( 16 downto 0 );
    c0_ddr4_ba : out STD_LOGIC_VECTOR ( 1 downto 0 );
    c0_ddr4_bg : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_cke : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_ck_t : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_ck_c : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_cs_n : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_dm_dbi_n : inout STD_LOGIC_VECTOR ( 7 downto 0 );
    c0_ddr4_dq : inout STD_LOGIC_VECTOR ( 63 downto 0 );
    c0_ddr4_dqs_c : inout STD_LOGIC_VECTOR ( 7 downto 0 );
    c0_ddr4_dqs_t : inout STD_LOGIC_VECTOR ( 7 downto 0 );
    c0_ddr4_odt : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_ddr4_reset_n : out STD_LOGIC_VECTOR ( 0 to 0 );
    c0_sys_clk_p : in STD_LOGIC_VECTOR ( 0 to 0 );
    c0_sys_clk_n : in STD_LOGIC_VECTOR ( 0 to 0 );
    UsrClk : out STD_LOGIC;
    sRst_Out : out STD_LOGIC;
    sys_rst : in STD_LOGIC;
    InitDone : out STD_LOGIC
  );

end design_1_vcu_ddr4_controller_0_0;

architecture stub of design_1_vcu_ddr4_controller_0_0 is
attribute syn_black_box : boolean;
attribute black_box_pad_pin : string;
attribute syn_black_box of stub : architecture is true;
attribute black_box_pad_pin of stub : architecture is "S_Axi_Clk,S_Axi_Rst,pl_barco_slot_wstrb0[15:0],pl_barco_slot_wstrb1[15:0],pl_barco_slot_wstrb2[15:0],pl_barco_slot_wstrb3[15:0],pl_barco_slot_wstrb4[15:0],pl_barco_slot_araddr0[31:0],pl_barco_slot_arburst0[1:0],pl_barco_slot_arid0[15:0],pl_barco_slot_arlen0[7:0],barco_pl_slot_arready0,pl_barco_slot_arsize0[2:0],pl_barco_slot_arvalid0,pl_barco_slot_awaddr0[31:0],pl_barco_slot_awburst0[1:0],pl_barco_slot_awid0[15:0],pl_barco_slot_awlen0[7:0],barco_pl_slot_awready0,pl_barco_slot_awsize0[2:0],pl_barco_slot_awvalid0,pl_barco_slot_bready0,barco_pl_slot_bvalid0,barco_pl_slot_bid0[15:0],barco_pl_slot_rdata0[127:0],barco_pl_slot_rid0[15:0],barco_pl_slot_rlast0,pl_barco_slot_rready0,barco_pl_slot_rvalid0,pl_barco_slot_wid0[15:0],pl_barco_slot_wdata0[127:0],pl_barco_slot_wlast0,barco_pl_slot_bresp0[1:0],barco_pl_slot_rresp0[1:0],barco_pl_slot_wready0,pl_barco_slot_wvalid0,pl_barco_slot_araddr1[31:0],pl_barco_slot_arburst1[1:0],pl_barco_slot_arid1[15:0],pl_barco_slot_arlen1[7:0],barco_pl_slot_arready1,pl_barco_slot_arsize1[2:0],pl_barco_slot_arvalid1,pl_barco_slot_awaddr1[31:0],pl_barco_slot_awburst1[1:0],pl_barco_slot_awid1[15:0],pl_barco_slot_awlen1[7:0],barco_pl_slot_awready1,pl_barco_slot_awsize1[2:0],pl_barco_slot_awvalid1,pl_barco_slot_bready1,barco_pl_slot_bvalid1,barco_pl_slot_bid1[15:0],barco_pl_slot_rdata1[127:0],barco_pl_slot_rid1[15:0],barco_pl_slot_rlast1,pl_barco_slot_rready1,barco_pl_slot_rvalid1,pl_barco_slot_wid1[15:0],pl_barco_slot_wdata1[127:0],pl_barco_slot_wlast1,barco_pl_slot_bresp1[1:0],barco_pl_slot_rresp1[1:0],barco_pl_slot_wready1,pl_barco_slot_wvalid1,pl_barco_slot_araddr2[31:0],pl_barco_slot_arburst2[1:0],pl_barco_slot_arid2[15:0],pl_barco_slot_arlen2[7:0],barco_pl_slot_arready2,pl_barco_slot_arsize2[2:0],pl_barco_slot_arvalid2,pl_barco_slot_awaddr2[31:0],pl_barco_slot_awburst2[1:0],pl_barco_slot_awid2[15:0],pl_barco_slot_awlen2[7:0],barco_pl_slot_awready2,pl_barco_slot_awsize2[2:0],pl_barco_slot_awvalid2,pl_barco_slot_bready2,barco_pl_slot_bvalid2,barco_pl_slot_bid2[15:0],barco_pl_slot_rdata2[127:0],barco_pl_slot_rid2[15:0],barco_pl_slot_rlast2,pl_barco_slot_rready2,barco_pl_slot_rvalid2,pl_barco_slot_wid2[15:0],pl_barco_slot_wdata2[127:0],pl_barco_slot_wlast2,barco_pl_slot_bresp2[1:0],barco_pl_slot_rresp2[1:0],barco_pl_slot_wready2,pl_barco_slot_wvalid2,pl_barco_slot_araddr3[31:0],pl_barco_slot_arburst3[1:0],pl_barco_slot_arid3[15:0],pl_barco_slot_arlen3[7:0],barco_pl_slot_arready3,pl_barco_slot_arsize3[2:0],pl_barco_slot_arvalid3,pl_barco_slot_awaddr3[31:0],pl_barco_slot_awburst3[1:0],pl_barco_slot_awid3[15:0],pl_barco_slot_awlen3[7:0],barco_pl_slot_awready3,pl_barco_slot_awsize3[2:0],pl_barco_slot_awvalid3,pl_barco_slot_bready3,barco_pl_slot_bvalid3,barco_pl_slot_bid3[15:0],barco_pl_slot_rdata3[127:0],barco_pl_slot_rid3[15:0],barco_pl_slot_rlast3,pl_barco_slot_rready3,barco_pl_slot_rvalid3,pl_barco_slot_wid3[15:0],pl_barco_slot_wdata3[127:0],pl_barco_slot_wlast3,barco_pl_slot_bresp3[1:0],barco_pl_slot_rresp3[1:0],barco_pl_slot_wready3,pl_barco_slot_wvalid3,pl_barco_slot_araddr4[31:0],pl_barco_slot_arburst4[1:0],pl_barco_slot_arid4[15:0],pl_barco_slot_arlen4[7:0],barco_pl_slot_arready4,pl_barco_slot_arsize4[2:0],pl_barco_slot_arvalid4,pl_barco_slot_awaddr4[31:0],pl_barco_slot_awburst4[1:0],pl_barco_slot_awid4[15:0],pl_barco_slot_awlen4[7:0],barco_pl_slot_awready4,pl_barco_slot_awsize4[2:0],pl_barco_slot_awvalid4,pl_barco_slot_bready4,barco_pl_slot_bvalid4,barco_pl_slot_bid4[15:0],barco_pl_slot_rdata4[127:0],barco_pl_slot_rid4[15:0],barco_pl_slot_rlast4,pl_barco_slot_rready4,barco_pl_slot_rvalid4,pl_barco_slot_wid4[15:0],pl_barco_slot_wdata4[127:0],pl_barco_slot_wlast4,barco_pl_slot_bresp4[1:0],barco_pl_slot_rresp4[1:0],barco_pl_slot_wready4,pl_barco_slot_wvalid4,c0_ddr4_act_n[0:0],c0_ddr4_adr[16:0],c0_ddr4_ba[1:0],c0_ddr4_bg[0:0],c0_ddr4_cke[0:0],c0_ddr4_ck_t[0:0],c0_ddr4_ck_c[0:0],c0_ddr4_cs_n[0:0],c0_ddr4_dm_dbi_n[7:0],c0_ddr4_dq[63:0],c0_ddr4_dqs_c[7:0],c0_ddr4_dqs_t[7:0],c0_ddr4_odt[0:0],c0_ddr4_reset_n[0:0],c0_sys_clk_p[0:0],c0_sys_clk_n[0:0],UsrClk,sRst_Out,sys_rst,InitDone";
attribute X_CORE_INFO : string;
attribute X_CORE_INFO of stub : architecture is "vcu_ddr4_controller_v1_0_1_ba317,Vivado 2019.1";
begin
end;
