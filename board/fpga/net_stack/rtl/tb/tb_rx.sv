/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

`timescale 1ns / 1ps

import axi4stream_vip_pkg::*;
import ex_sim_axi4stream_vip_tx_payload_0_pkg::*;
import ex_sim_axi4stream_vip_tx_hdr_0_pkg::*;
import ex_sim_axi4stream_vip_tx_setconn_0_pkg::*;

module testbench_netstack;

parameter CLK_PERIOD = 4;
parameter SEQ_WIDTH = 4;

typedef enum logic [7:0] {
	pkt_ack=1,
	pkt_nack=2,
	pkt_data=3,
	pkt_syn=4,
	pkt_fin=5
} pkt_type_t;

axi4stream_transaction wr_trans;
axi4stream_transaction wr_hdr_trans;
axi4stream_transaction wr_set_trans;

ex_sim_axi4stream_vip_tx_payload_0_mst_t	tx_agent;
ex_sim_axi4stream_vip_tx_hdr_0_mst_t		tx_hdr_agent;
ex_sim_axi4stream_vip_tx_setconn_0_mst_t	tx_set_agent;

bit clk;
bit rst;

bit enable_rst;
bit enable_send;
bit enable_setconn;
bit enable_close;

bit [31:0] sequence_num;

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

wire [111:0] tx_udp_hdr_data;
wire tx_udp_hdr_valid;
wire tx_udp_hdr_ready;
wire [63:0] tx_udp_payload_axis_tdata;
wire [7:0] tx_udp_payload_axis_tkeep;
wire tx_udp_payload_axis_tvalid;
wire tx_udp_payload_axis_tready;
wire tx_udp_payload_axis_tlast;
wire tx_udp_payload_axis_tuser;

wire [111:0] usr_rx_hdr_V_TDATA;
wire usr_rx_hdr_V_TREADY;
wire usr_rx_hdr_V_TVALID;
wire [63:0] usr_rx_payload_TDATA;
wire [7:0] usr_rx_payload_TKEEP;
wire usr_rx_payload_TLAST;
wire usr_rx_payload_TREADY;
wire usr_rx_payload_TUSER;
wire usr_rx_payload_TVALID;

wire [15:0] setconn_axis_tdata;
wire setconn_axis_tready;
wire setconn_axis_tvalid;

ex_sim ex_host (
	.M_AXIS_hdr_tdata(tx_udp_hdr_data),
	.M_AXIS_hdr_tready(tx_udp_hdr_ready),
	.M_AXIS_hdr_tvalid(tx_udp_hdr_valid),
	.M_AXIS_payload_tdata(tx_udp_payload_axis_tdata),
	.M_AXIS_payload_tkeep(tx_udp_payload_axis_tkeep),
	.M_AXIS_payload_tlast(tx_udp_payload_axis_tlast),
	.M_AXIS_payload_tready(tx_udp_payload_axis_tready),
	.M_AXIS_payload_tvalid(tx_udp_payload_axis_tvalid),
	.M_AXIS_setconn_tdata(),
	.M_AXIS_setconn_tready(1'b1),
	.M_AXIS_setconn_tvalid(),
	.aclk(clk),
	.aresetn(~rst)
);

assign tx_udp_payload_axis_tuser = 1'b0;

host_stack master (
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
	// UDP test input
	.s_udp_hdr_data(tx_udp_hdr_data),
	.s_udp_hdr_ready(tx_udp_hdr_ready),
	.s_udp_hdr_valid(tx_udp_hdr_valid),
	.s_udp_payload_axis_tdata(tx_udp_payload_axis_tdata),
	.s_udp_payload_axis_tkeep(tx_udp_payload_axis_tkeep),
	.s_udp_payload_axis_tlast(tx_udp_payload_axis_tlast),
	.s_udp_payload_axis_tready(tx_udp_payload_axis_tready),
	.s_udp_payload_axis_tvalid(tx_udp_payload_axis_tvalid),
	.s_udp_payload_axis_tuser(tx_udp_payload_axis_tuser)
);

fpga_core #(
	.INTEGRATION_MODE(1)
)
relnet_core (
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

	// onboard pipeline output
	.m_usr_hdr_data(usr_rx_hdr_V_TDATA),
	.m_usr_hdr_valid(usr_rx_hdr_V_TVALID),
	.m_usr_hdr_ready(usr_rx_hdr_V_TREADY),
	.m_usr_payload_axis_tdata(usr_rx_payload_TDATA),
	.m_usr_payload_axis_tvalid(usr_rx_payload_TVALID),
	.m_usr_payload_axis_tready(usr_rx_payload_TREADY),
	.m_usr_payload_axis_tlast(usr_rx_payload_TLAST),
	.m_usr_payload_axis_tkeep(usr_rx_payload_TKEEP),
	.m_usr_payload_axis_tuser(usr_rx_payload_TUSER),

	// set connection input
	.s_setconn_axis_tdata(setconn_axis_tdata),
	.s_setconn_axis_tvalid(setconn_axis_tvalid),
	.s_setconn_axis_tready(setconn_axis_tready),

	// identity info
	.local_ip({>>{8'd192, 8'd168, 8'd1,   8'd128}})
);

dummy_setup_inst
relnet_setup (
	.ap_clk(clk),                                    // input wire ap_clk
	.ap_rst_n(~rst),                                // input wire ap_rst_n
	.usr_rx_payload_TVALID(usr_rx_payload_TVALID),      // input wire usr_rx_payload_TVALID
	.usr_rx_payload_TREADY(usr_rx_payload_TREADY),      // output wire usr_rx_payload_TREADY
	.usr_rx_payload_TDATA(usr_rx_payload_TDATA),        // input wire [63 : 0] usr_rx_payload_TDATA
	.usr_rx_payload_TUSER(usr_rx_payload_TUSER),        // input wire [0 : 0] usr_rx_payload_TUSER
	.usr_rx_payload_TLAST(usr_rx_payload_TLAST),        // input wire [0 : 0] usr_rx_payload_TLAST
	.usr_rx_payload_TKEEP(usr_rx_payload_TKEEP),        // input wire [7 : 0] usr_rx_payload_TKEEP
	.usr_rx_hdr_V_TVALID(usr_rx_hdr_V_TVALID),          // input wire usr_rx_hdr_V_TVALID
	.usr_rx_hdr_V_TREADY(usr_rx_hdr_V_TREADY),          // output wire usr_rx_hdr_V_TREADY
	.usr_rx_hdr_V_TDATA(usr_rx_hdr_V_TDATA),            // input wire [111 : 0] usr_rx_hdr_V_TDATA
	.conn_setup_req_V_TVALID(setconn_axis_tvalid),  // output wire conn_setup_req_V_TVALID
	.conn_setup_req_V_TREADY(setconn_axis_tready),  // input wire conn_setup_req_V_TREADY
	.conn_setup_req_V_TDATA(setconn_axis_tdata)    // output wire [15 : 0] conn_setup_req_V_TDATA
);

always #CLK_PERIOD clk <= ~clk;

// initialize transaction
initial begin

	enable_send <= 1'b0;
	enable_rst <= 1'b0;
	enable_setconn <= 1'b0;
	enable_close <= 1'b0;

	$display("initilization start");

	tx_agent = new("tx pld agent",ex_host.axi4stream_vip_tx_payload.inst.IF);
	tx_hdr_agent = new("tx hdr agent",ex_host.axi4stream_vip_tx_hdr.inst.IF);
	tx_set_agent = new("set conn agent",ex_host.axi4stream_vip_tx_setconn.inst.IF);

	tx_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	tx_hdr_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	tx_set_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);

	wr_trans = tx_agent.driver.create_transaction("tx pld trans");
	wr_hdr_trans = tx_hdr_agent.driver.create_transaction("tx hdr trans");
	wr_set_trans = tx_set_agent.driver.create_transaction("set conn trans");

	tx_agent.start_master();
	tx_hdr_agent.start_master();
	tx_set_agent.start_master();
	
	enable_rst <= 1'b1;
end

initial begin
	wait(enable_rst == 1'b1);
	rst <= 1'b0;
	sequence_num <= 32'd1;

	#20
	rst <= 1'b1;

	#160
	rst <= 1'b0;

	#20
	enable_setconn <= 1'b1;
end

// send gbn SYN packet to init a connection 
initial begin
	wait(enable_setconn == 1'b1);

	/*
	 * somehow you have to do this otherwise it will output X
	 */
	assert(wr_trans.randomize());
	assert(wr_hdr_trans.randomize());

	src_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd2}};
	dest_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd128}};
	src_port = {<<8{16'd1000}};
	dest_port = {<<8{16'd1234}};
	length = {<<8{16'd16}};

	session_id = {<<8{8'd20, 8'd0, 8'd0}};  // {10'd20(src slot), 10'd0(dest slot), 4'd0}

	wr_hdr_trans.set_data({length, dest_port, src_port, dest_ip, src_ip});

	#CLK_PERIOD;
	data = {<<8{session_id, 32'd0, pkt_syn}};
	wr_trans.set_data(data);
	wr_trans.set_last(1'b0);
	$display("send SYN udp head");
	tx_hdr_agent.driver.send(wr_hdr_trans);
	$display("send SYN gbn head");
	tx_agent.driver.send(wr_trans);

	#CLK_PERIOD;
	$display("send SYN udp data");
	{>>{data}} = 64'h0101010101010101;
	wr_trans.set_data(data);
	wr_trans.set_last(1'b1);
	tx_agent.driver.send(wr_trans);

	#200;
	enable_send <= 1'b1;
end

//bit [31:0] test_seq [7] = {1, 2, 3, 4, 5, 6, 7};
//bit [31:0] test_seq [6] = {1, 2, 3, 5, 6, 7};
//bit [31:0] test_seq [8] = {1, 2, 3, 5, 6, 7, 4, 8};
bit [31:0] test_seq [10] = {1, 2, 3, 5, 6, 4, 6, 5, 6, 5};

//pkt_type_t typ = pkt_data;
pkt_type_t typ[10] = {1, 4, 2, 2, 1, 1, 2, 1, 2, 1};

// send gbn header and udp header
initial begin
	wait(enable_send == 1'b1);

	src_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd2}};
	dest_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd128}};
	src_port = {<<8{16'd1000}};
	dest_port = {<<8{16'd1234}};
	length = {<<8{16'd24}};	// 2*64bit

	session_id = {<<8{8'd20, 8'd40, 8'd0}};  // {10'd20(src slot), 10'd10(dest slot), 4'd0}

	wr_hdr_trans.set_data({length, dest_port, src_port, dest_ip, src_ip});

	for (int i = 0; i < $size(test_seq); i++) begin
		{>>{seqnum}} <= test_seq[i];

		#CLK_PERIOD;
		data = {<<8{session_id, seqnum, typ[i]}};
		wr_trans.set_data(data);
		wr_trans.set_last(1'b0);
		$display("send udp head");
		tx_hdr_agent.driver.send(wr_hdr_trans);
		$display("send gbn head");
		tx_agent.driver.send(wr_trans);

		$display("send udp data");
		#CLK_PERIOD;
		{>>{data}} = 64'h0f0f0f0f0f0f0f0f;
		wr_trans.set_data(data);
		wr_trans.set_last(1'b0);
		tx_agent.driver.send(wr_trans);

		#CLK_PERIOD;
		{>>{data}} = 64'h0101010101010101;
		wr_trans.set_data(data);
		wr_trans.set_last(1'b1);
		tx_agent.driver.send(wr_trans);
	end

	#CLK_PERIOD;
	enable_close <= 1'b1;
end

initial begin
	wait(enable_close == 1'b1);

	src_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd2}};
	dest_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd128}};
	src_port = {<<8{16'd1000}};
	dest_port = {<<8{16'd1234}};
	length = {<<8{16'd16}};

	session_id = {<<8{8'd0, 8'd40, 8'd0}};

	wr_hdr_trans.set_data({length, dest_port, src_port, dest_ip, src_ip});

	#CLK_PERIOD;
	data = {<<8{session_id, 32'd0, pkt_fin}};
	wr_trans.set_data(data);
	wr_trans.set_last(1'b0);
	$display("send FIN udp head");
	tx_hdr_agent.driver.send(wr_hdr_trans);
	$display("send FIN gbn head");
	tx_agent.driver.send(wr_trans);

	#CLK_PERIOD;
	$display("send FIN udp data");
	{>>{data}} = 64'h0101010101010101;
	wr_trans.set_data(data);
	wr_trans.set_last(1'b1);
	tx_agent.driver.send(wr_trans);
end

endmodule // testbench_netstack
