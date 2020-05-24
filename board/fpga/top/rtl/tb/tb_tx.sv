/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

`timescale 1ns / 1ps

import axi4stream_vip_pkg::*;
import ex_sim_axi4stream_vip_tx_payload_0_pkg::*;
import ex_sim_axi4stream_vip_tx_hdr_0_pkg::*;
import ex_sim_axi4stream_vip_tx_setconn_0_pkg::*;
import ex_sim_axi4stream_vip_usr_hdr_0_pkg::*;
import ex_sim_axi4stream_vip_usr_payload_0_pkg::*;

module testbench_tx;

parameter CLK_PERIOD = 4;
parameter SEQ_WIDTH = 4;

typedef enum logic [7:0] {
	pkt_ack=1,
	pkt_nack=2,
	pkt_data=3
} pkt_type_t;

axi4stream_transaction pld_trans;
axi4stream_transaction usr_pld_trans;
axi4stream_transaction hdr_trans;
axi4stream_transaction usr_hdr_trans;
axi4stream_transaction ctrl_trans;

ex_sim_axi4stream_vip_tx_payload_0_mst_t	pld_agent;
ex_sim_axi4stream_vip_tx_hdr_0_mst_t		hdr_agent;
ex_sim_axi4stream_vip_tx_setconn_0_mst_t	ctrl_agent;
ex_sim_axi4stream_vip_usr_hdr_0_mst_t		usr_hdr_agent;
ex_sim_axi4stream_vip_usr_payload_0_mst_t	usr_pld_agent;

bit clk;
bit rst;

bit enable_rst;
bit enable_send;
bit enable_setconn;
bit enable_retrans;

bit [31:0] sequence_num;
logic [7:0] mst_ip[4];
logic [7:0] mst_mac[6];
logic [7:0] slv_ip[4];
logic [7:0] slv_mac[6];

logic [7:0] src_ip[4];
logic [7:0] dest_ip[4];
logic [7:0] src_port[2];
logic [7:0] dest_port[2];
logic [7:0] length[2];

logic [7:0] seqnum[SEQ_WIDTH];
logic [7:0] session_id[3];
logic [7:0] data[8];
logic [7:0] set_req[2];

wire [63:0] m_txd_1;
wire [7:0] m_txc_1;
wire [63:0] m_txd_2;
wire [7:0] m_txc_2;
wire [63:0] s_txd_1;
wire [7:0] s_txc_1;
wire [63:0] s_txd_2;
wire [7:0] s_txc_2;

// connection to relnet core
wire [111:0] in_udp_hdr_data;
wire in_udp_hdr_valid;
wire in_udp_hdr_ready;
wire [63:0] in_udp_payload_axis_tdata;
wire [7:0] in_udp_payload_axis_tkeep;
wire in_udp_payload_axis_tvalid;
wire in_udp_payload_axis_tready;
wire in_udp_payload_axis_tlast;
wire in_udp_payload_axis_tuser;

wire [111:0] out_udp_hdr_data;
wire out_udp_hdr_valid;
wire out_udp_hdr_ready;
wire [63:0] out_udp_payload_axis_tdata;
wire [7:0] out_udp_payload_axis_tkeep;
wire out_udp_payload_axis_tvalid;
wire out_udp_payload_axis_tready;
wire out_udp_payload_axis_tlast;
wire out_udp_payload_axis_tuser;

wire [31:0] out_udp_ip_source_ip;
wire [31:0] out_udp_ip_dest_ip;
wire [15:0] out_udp_source_port;
wire [15:0] out_udp_dest_port;
wire [15:0] out_udp_length;

wire [31:0] usr_rx_udp_ip_source_ip;
wire [31:0] usr_rx_udp_ip_dest_ip;
wire [15:0] usr_rx_udp_source_port;
wire [15:0] usr_rx_udp_dest_port;
wire [15:0] usr_rx_udp_length;

wire [111:0] usr_rx_hdr_V_TDATA;
wire usr_rx_hdr_V_TREADY;
wire usr_rx_hdr_V_TVALID;
wire [63:0] usr_rx_payload_TDATA;
wire [7:0] usr_rx_payload_TKEEP;
wire usr_rx_payload_TLAST;
wire usr_rx_payload_TREADY;
wire usr_rx_payload_TUSER;
wire usr_rx_payload_TVALID;

wire [111:0] usr_tx_hdr_V_TDATA;
wire usr_tx_hdr_V_TREADY;
wire usr_tx_hdr_V_TVALID;
wire [63:0] usr_tx_payload_TDATA;
wire [7:0] usr_tx_payload_TKEEP;
wire usr_tx_payload_TLAST;
wire usr_tx_payload_TREADY;
wire usr_tx_payload_TUSER;
wire usr_tx_payload_TVALID;

wire [15:0] setconn_axis_tdata;
wire setconn_axis_tready;
wire setconn_axis_tvalid;

wire [111:0] slv_rx_udp_hdr_data;
wire slv_rx_udp_hdr_valid;
wire slv_rx_udp_hdr_ready;
wire [63:0] slv_rx_udp_payload_axis_tdata;
wire [7:0] slv_rx_udp_payload_axis_tkeep;
wire slv_rx_udp_payload_axis_tvalid;
wire slv_rx_udp_payload_axis_tready;
wire slv_rx_udp_payload_axis_tlast;
wire slv_rx_udp_payload_axis_tuser;

wire [111:0] slv_tx_udp_hdr_data;
wire slv_tx_udp_hdr_valid;
wire slv_tx_udp_hdr_ready;
wire [63:0] slv_tx_udp_payload_axis_tdata;
wire [7:0] slv_tx_udp_payload_axis_tkeep;
wire slv_tx_udp_payload_axis_tvalid;
wire slv_tx_udp_payload_axis_tready;
wire slv_tx_udp_payload_axis_tlast;
wire slv_tx_udp_payload_axis_tuser;

ex_sim ex_host (
	.M_AXIS_hdr_tdata(slv_tx_udp_hdr_data),
	.M_AXIS_hdr_tready(slv_tx_udp_hdr_ready),
	.M_AXIS_hdr_tvalid(slv_tx_udp_hdr_valid),
	.M_AXIS_payload_tdata(slv_tx_udp_payload_axis_tdata),
	.M_AXIS_payload_tkeep(slv_tx_udp_payload_axis_tkeep),
	.M_AXIS_payload_tlast(slv_tx_udp_payload_axis_tlast),
	.M_AXIS_payload_tready(slv_tx_udp_payload_axis_tready),
	.M_AXIS_payload_tvalid(slv_tx_udp_payload_axis_tvalid),
	.M_AXIS_setconn_tdata(setconn_axis_tdata),
	.M_AXIS_setconn_tready(setconn_axis_tready),
	.M_AXIS_setconn_tvalid(setconn_axis_tvalid),
	.M_AXIS_usr_hdr_tdata(usr_tx_hdr_V_TDATA),
	.M_AXIS_usr_hdr_tready(usr_tx_hdr_V_TREADY),
	.M_AXIS_usr_hdr_tvalid(usr_tx_hdr_V_TVALID),
	.M_AXIS_usr_payload_tdata(usr_tx_payload_TDATA),
	.M_AXIS_usr_payload_tkeep(usr_tx_payload_TKEEP),
	.M_AXIS_usr_payload_tlast(usr_tx_payload_TLAST),
	.M_AXIS_usr_payload_tready(usr_tx_payload_TREADY),
	.M_AXIS_usr_payload_tvalid(usr_tx_payload_TVALID),
	.aclk(clk),
	.aresetn(~rst)
);

assign {
	out_udp_length,
	out_udp_dest_port,
	out_udp_source_port,
	out_udp_ip_dest_ip,
	out_udp_ip_source_ip
} = out_udp_hdr_data;

assign {
	usr_rx_udp_length,
	usr_rx_udp_dest_port,
	usr_rx_udp_source_port,
	usr_rx_udp_ip_dest_ip,
	usr_rx_udp_ip_source_ip
} = usr_rx_hdr_V_TDATA;

assign usr_rx_hdr_V_TREADY = 1;
assign usr_rx_payload_TREADY = 1;
assign slv_rx_udp_payload_axis_tready = 1;
assign slv_rx_udp_hdr_ready = 1;
assign in_udp_payload_axis_tuser = 0;
assign usr_tx_payload_TUSER = 0;
assign slv_tx_udp_payload_axis_tuser = 0;

relnet
relnet_inst (
	.ap_clk(clk),
	.ap_rst_n(~rst),
	// UDP frame input
	.in_header_tdata(in_udp_hdr_data),
	.in_header_tready(in_udp_hdr_ready),
	.in_header_tvalid(in_udp_hdr_valid),
	.in_payload_tdata(in_udp_payload_axis_tdata),
	.in_payload_tkeep(in_udp_payload_axis_tkeep),
	.in_payload_tlast(in_udp_payload_axis_tlast),
	.in_payload_tready(in_udp_payload_axis_tready),
	.in_payload_tuser(in_udp_payload_axis_tuser),
	.in_payload_tvalid(in_udp_payload_axis_tvalid),
	// UDP frame output
	.out_header_tdata(out_udp_hdr_data),
	.out_header_tready(out_udp_hdr_ready),
	.out_header_tvalid(out_udp_hdr_valid),
	.out_payload_tdata(out_udp_payload_axis_tdata),
	.out_payload_tkeep(out_udp_payload_axis_tkeep),
	.out_payload_tlast(out_udp_payload_axis_tlast),
	.out_payload_tready(out_udp_payload_axis_tready),
	.out_payload_tuser(out_udp_payload_axis_tuser),
	.out_payload_tvalid(out_udp_payload_axis_tvalid),
	// onboard pipeline output
	.usr_rx_header_tdata(usr_rx_hdr_V_TDATA),
	.usr_rx_header_tready(usr_rx_hdr_V_TREADY),
	.usr_rx_header_tvalid(usr_rx_hdr_V_TVALID),
	.usr_rx_payload_tdata(usr_rx_payload_TDATA),
	.usr_rx_payload_tkeep(usr_rx_payload_TKEEP),
	.usr_rx_payload_tlast(usr_rx_payload_TLAST),
	.usr_rx_payload_tready(usr_rx_payload_TREADY),
	.usr_rx_payload_tuser(usr_rx_payload_TUSER),
	.usr_rx_payload_tvalid(usr_rx_payload_TVALID),
	// onboard pipeline input
	.usr_tx_header_tdata(usr_tx_hdr_V_TDATA),
	.usr_tx_header_tready(usr_tx_hdr_V_TREADY),
	.usr_tx_header_tvalid(usr_tx_hdr_V_TVALID),
	.usr_tx_payload_tdata(usr_tx_payload_TDATA),
	.usr_tx_payload_tkeep(usr_tx_payload_TKEEP),
	.usr_tx_payload_tlast(usr_tx_payload_TLAST),
	.usr_tx_payload_tready(usr_tx_payload_TREADY),
	.usr_tx_payload_tuser(usr_tx_payload_TUSER),
	.usr_tx_payload_tvalid(usr_tx_payload_TVALID),
	// connection management input
	.conn_set_req_tdata(setconn_axis_tdata),
	.conn_set_req_tready(setconn_axis_tready),
	.conn_set_req_tvalid(setconn_axis_tvalid)
);

fpga_core relnet_core_mst (
	.clk(clk),
	.rst(rst),
	// Ethernet: SFP+
	.sfp_1_tx_clk(clk),
	.sfp_1_tx_rst(rst),
	.sfp_1_txd(m_txd_1),
	.sfp_1_txc(m_txc_1),
	.sfp_1_rx_clk(clk),
	.sfp_1_rx_rst(rst),
	.sfp_1_rxd(s_txd_1),
	.sfp_1_rxc(s_txc_1),
	.sfp_2_tx_clk(clk),
	.sfp_2_tx_rst(rst),
	.sfp_2_txd(m_txd_2),
	.sfp_2_txc(m_txc_2),
	.sfp_2_rx_clk(clk),
	.sfp_2_rx_rst(rst),
	.sfp_2_rxd(s_txd_2),
	.sfp_2_rxc(s_txc_2),

	// onboard pipeline input
	.s_udp_hdr_data(out_udp_hdr_data),
	.s_udp_hdr_valid(out_udp_hdr_valid),
	.s_udp_hdr_ready(out_udp_hdr_ready),
	.s_udp_payload_axis_tdata(out_udp_payload_axis_tdata),
	.s_udp_payload_axis_tvalid(out_udp_payload_axis_tvalid),
	.s_udp_payload_axis_tready(out_udp_payload_axis_tready),
	.s_udp_payload_axis_tlast(out_udp_payload_axis_tlast),
	.s_udp_payload_axis_tkeep(out_udp_payload_axis_tkeep),
	.s_udp_payload_axis_tuser(out_udp_payload_axis_tuser),

	// onboard pipeline output
	.m_udp_hdr_data(in_udp_hdr_data),
	.m_udp_hdr_valid(in_udp_hdr_valid),
	.m_udp_hdr_ready(in_udp_hdr_ready),
	.m_udp_payload_axis_tdata(in_udp_payload_axis_tdata),
	.m_udp_payload_axis_tvalid(in_udp_payload_axis_tvalid),
	.m_udp_payload_axis_tready(in_udp_payload_axis_tready),
	.m_udp_payload_axis_tlast(in_udp_payload_axis_tlast),
	.m_udp_payload_axis_tkeep(in_udp_payload_axis_tkeep),
	.m_udp_payload_axis_tuser(in_udp_payload_axis_tuser),

	// identity info
	.local_ip({>>{mst_ip}}),
	.local_mac({>>{mst_mac}})
);

fpga_core relnet_core_slv (
	.clk(clk),
	.rst(rst),
	// Ethernet: SFP+
	.sfp_1_tx_clk(clk),
	.sfp_1_tx_rst(rst),
	.sfp_1_txd(s_txd_1),
	.sfp_1_txc(s_txc_1),
	.sfp_1_rx_clk(clk),
	.sfp_1_rx_rst(rst),
	.sfp_1_rxd(m_txd_1),
	.sfp_1_rxc(m_txc_1),
	.sfp_2_tx_clk(clk),
	.sfp_2_tx_rst(rst),
	.sfp_2_txd(s_txd_2),
	.sfp_2_txc(s_txc_2),
	.sfp_2_rx_clk(clk),
	.sfp_2_rx_rst(rst),
	.sfp_2_rxd(m_txd_2),
	.sfp_2_rxc(m_txc_2),

	// onboard pipeline input
	.s_udp_hdr_data(slv_tx_udp_hdr_data),
	.s_udp_hdr_valid(slv_tx_udp_hdr_valid),
	.s_udp_hdr_ready(slv_tx_udp_hdr_ready),
	.s_udp_payload_axis_tdata(slv_tx_udp_payload_axis_tdata),
	.s_udp_payload_axis_tvalid(slv_tx_udp_payload_axis_tvalid),
	.s_udp_payload_axis_tready(slv_tx_udp_payload_axis_tready),
	.s_udp_payload_axis_tlast(slv_tx_udp_payload_axis_tlast),
	.s_udp_payload_axis_tkeep(slv_tx_udp_payload_axis_tkeep),
	.s_udp_payload_axis_tuser(slv_tx_udp_payload_axis_tuser),

	// onboard pipeline output
	.m_udp_hdr_data(slv_rx_udp_hdr_data),
	.m_udp_hdr_valid(slv_rx_udp_hdr_valid),
	.m_udp_hdr_ready(slv_rx_udp_hdr_ready),
	.m_udp_payload_axis_tdata(slv_rx_udp_payload_axis_tdata),
	.m_udp_payload_axis_tvalid(slv_rx_udp_payload_axis_tvalid),
	.m_udp_payload_axis_tready(slv_rx_udp_payload_axis_tready),
	.m_udp_payload_axis_tlast(slv_rx_udp_payload_axis_tlast),
	.m_udp_payload_axis_tkeep(slv_rx_udp_payload_axis_tkeep),
	.m_udp_payload_axis_tuser(slv_rx_udp_payload_axis_tuser),

	// identity info
	.local_ip({>>{slv_ip}}),
	.local_mac({>>{slv_mac}})
);
always #CLK_PERIOD clk <= ~clk;

// initialize transaction
initial begin

	enable_send <= 1'b0;
	enable_rst <= 1'b0;
	enable_setconn <= 1'b0;
	enable_retrans <= 1'b0;

	$display("initilization start");

	pld_agent = new("tx pld agent",ex_host.axi4stream_vip_tx_payload.inst.IF);
	hdr_agent = new("tx hdr agent",ex_host.axi4stream_vip_tx_hdr.inst.IF);
	ctrl_agent = new("set conn agent",ex_host.axi4stream_vip_tx_setconn.inst.IF);
	// slv_axi_agent = new("slv mem agent",relnet_inst.axi_vip.inst.IF);

	usr_pld_agent = new("tx usr pld agent",ex_host.axi4stream_vip_usr_payload.inst.IF);
	usr_hdr_agent = new("tx usr hdr agent",ex_host.axi4stream_vip_usr_hdr.inst.IF);

	pld_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	hdr_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	ctrl_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);

	usr_pld_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	usr_hdr_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);

	pld_trans = pld_agent.driver.create_transaction("tx pld trans");
	hdr_trans = hdr_agent.driver.create_transaction("tx hdr trans");
	ctrl_trans = ctrl_agent.driver.create_transaction("set conn trans");

	usr_pld_trans = usr_pld_agent.driver.create_transaction("tx usr pld trans");
	usr_hdr_trans = usr_hdr_agent.driver.create_transaction("tx usr hdr trans");

	pld_agent.start_master();
	hdr_agent.start_master();
	ctrl_agent.start_master();
	usr_hdr_agent.start_master();
	usr_pld_agent.start_master();
	// slv_axi_agent.start_slave();
	
	enable_rst <= 1'b1;
end

initial begin
	wait(enable_rst == 1'b1);
	rst <= 1'b0;

	#20
	rst <= 1'b1;

	#160
	rst <= 1'b0;

	#20
	mst_ip <= {8'd192, 8'd168, 8'd1,   8'd128};
	mst_mac <= {8'd11, 8'd22, 8'd33, 8'd44, 8'd55, 8'd66};
	slv_ip <= {8'd192, 8'd168, 8'd1,   8'd2};
	slv_mac <= {8'd11, 8'd22, 8'd33, 8'd44, 8'd55, 8'd77};
	enable_setconn <= 1'b1;
end

initial begin
	wait(enable_setconn == 1'b1);

	/*
	 * somehow you have to do this otherwise it will output X
	 */
	assert(pld_trans.randomize());
	assert(hdr_trans.randomize());
	assert(ctrl_trans.randomize());
	assert(usr_pld_trans.randomize());
	assert(usr_hdr_trans.randomize());

	// set connection state in master side: slot 20
	set_req = '{8'd10, 8'd4};  // {10'd20(slot id), 6'd1(type GBN_SOC2FPGA_SET_TYPE_OPEN)}

	ctrl_trans.set_data(set_req);
	ctrl_agent.driver.send(ctrl_trans);

	#200;
	enable_send <= 1'b1;
end

bit [31:0] test_seq [7] = {1, 2, 3, 4, 5, 6, 7};

// send gbn header and udp header
initial begin
	wait(enable_send == 1'b1);

	src_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd2}};
	dest_ip = {<<8{8'd1,   8'd128, 8'd10, 8'd10}};
	src_port = {<<8{16'd1000}};
	dest_port = {<<8{16'd1234}};
	length = {<<8{16'd24}};	// 5*64bit

	session_id = {<<8{8'd10, 8'd40, 8'd0}};  // {10'd10(src slot), 10'd10(dest slot), 4'd0}

	hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	{>>{seqnum}} = {<<8{32'd0}};

	for (int i = 0; i < 15; i++) begin
		seqnum[3]++;

		data = {<<8{session_id, seqnum, pkt_data}};
		pld_trans.set_data(data);
		pld_trans.set_last(1'b0);
		$display("send udp head");
		hdr_agent.driver.send(hdr_trans);
		$display("send gbn head");
		pld_agent.driver.send(pld_trans);

		$display("send udp data");
		{>>{data}} = 64'h0f0f0f0f0f0f0f0f;
		pld_trans.set_data(data);
		pld_trans.set_last(1'b0);

		for (int j = 0; j < 3; j++) begin
			pld_agent.driver.send(pld_trans);
		end

		{>>{data}} <= 64'h0101010101010101;
		pld_trans.set_data(data);
		pld_trans.set_last(1'b1);
		pld_agent.driver.send(pld_trans);
	end

	#200;
	// enable_retrans <= 1'b1;
end

initial begin
	wait(enable_send == 1'b1);

	src_ip = {<<8{8'd192, 8'd168, 8'd1, 8'd128}};
	dest_ip = {<<8{8'd1,   8'd2, 8'd10, 8'd10}};
	src_port = {<<8{16'd10}};
	dest_port = {<<8{16'd10}};
	length = {<<8{16'd168}};

	usr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	for (int i = 0; i < 7; i++) begin
		$display("send udp head");
		usr_hdr_agent.driver.send(usr_hdr_trans);

		$display("send udp data");
		{>>{data}} = 64'h0f0f0f0f0f0f0f0f;
		usr_pld_trans.set_data(data);
		usr_pld_trans.set_last(1'b0);

		for (int j = 0; j < 20; j++) begin
			usr_pld_agent.driver.send(usr_pld_trans);
		end

		{>>{data}} = 64'h0101010101010101;
		usr_pld_trans.set_data(data);
		usr_pld_trans.set_last(1'b1);
		usr_pld_agent.driver.send(usr_pld_trans);
	end
end

always @(posedge clk) begin
	if (out_udp_hdr_valid && out_udp_hdr_ready) begin
		$display("master receive udp header: %h:%d -> %h:%d",
			out_udp_ip_source_ip,
			out_udp_source_port,
			out_udp_ip_dest_ip,
			out_udp_dest_port);
	end
	if (out_udp_payload_axis_tvalid && out_udp_payload_axis_tready) begin
		$display("master receive data: %x", out_udp_payload_axis_tdata);
	end
end

always @(posedge clk) begin
	if (usr_rx_hdr_V_TVALID && usr_rx_hdr_V_TREADY) begin
		$display("user receive udp header: %h:%d -> %h:%d",
			usr_rx_udp_ip_source_ip,
			usr_rx_udp_source_port,
			usr_rx_udp_ip_dest_ip,
			usr_rx_udp_dest_port);
	end
	if (usr_rx_payload_TVALID && usr_rx_payload_TREADY) begin
		$display("user receive data: %x", usr_rx_payload_TDATA);
	end
end

endmodule
