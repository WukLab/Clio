/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

`timescale 1ns / 1ps

import axi4stream_vip_pkg::*;
import axi_vip_pkg::*;
import ex_sim_axi4stream_vip_tx_payload_0_pkg::*;
import ex_sim_axi4stream_vip_tx_hdr_0_pkg::*;
import ex_sim_axi4stream_vip_tx_setconn_0_pkg::*;
import relnet_axi_vip_0_pkg::*;

module testbench_netstack_3;

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
relnet_axi_vip_0_slv_mem_t			slv_axi_agent;

bit clk;
bit rst;

bit enable_rst;
bit enable_send;
bit enable_setconn;
bit enable_retrans;

bit [31:0] sequence_num;
logic [7:0] mst_ip[4];
logic [7:0] slv_ip[4];

logic [7:0] src_ip[4];
logic [7:0] dest_ip[4];
logic [7:0] src_port[2];
logic [7:0] dest_port[2];
logic [7:0] length[2];

logic [7:0] seqnum[SEQ_WIDTH];
logic [7:0] session_id[3];
logic [7:0] data[8];
logic [7:0] set_req[2];

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

ex_sim ex_host (
	.M_AXIS_hdr_tdata(in_udp_hdr_data),
	.M_AXIS_hdr_tready(in_udp_hdr_ready),
	.M_AXIS_hdr_tvalid(in_udp_hdr_valid),
	.M_AXIS_payload_tdata(in_udp_payload_axis_tdata),
	.M_AXIS_payload_tkeep(in_udp_payload_axis_tkeep),
	.M_AXIS_payload_tlast(in_udp_payload_axis_tlast),
	.M_AXIS_payload_tready(in_udp_payload_axis_tready),
	.M_AXIS_payload_tvalid(in_udp_payload_axis_tvalid),
	.M_AXIS_setconn_tdata(),
	.M_AXIS_setconn_tready(1'b1),
	.M_AXIS_setconn_tvalid(),
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

assign out_udp_hdr_ready = 1;
assign out_udp_payload_axis_tready = 1;
assign in_udp_payload_axis_tuser = 0;

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
	.conn_setup_req_V_TDATA(setconn_axis_tdata),    // output wire [15 : 0] conn_setup_req_V_TDATA
	.usr_tx_payload_TVALID(usr_tx_payload_TVALID),      // output wire usr_tx_payload_TVALID
	.usr_tx_payload_TREADY(usr_tx_payload_TREADY),      // input wire usr_tx_payload_TREADY
	.usr_tx_payload_TDATA(usr_tx_payload_TDATA),        // output wire [63 : 0] usr_tx_payload_TDATA
	.usr_tx_payload_TUSER(usr_tx_payload_TUSER),        // output wire [0 : 0] usr_tx_payload_TUSER
	.usr_tx_payload_TLAST(usr_tx_payload_TLAST),        // output wire [0 : 0] usr_tx_payload_TLAST
	.usr_tx_payload_TKEEP(usr_tx_payload_TKEEP),        // output wire [7 : 0] usr_tx_payload_TKEEP
	.usr_tx_hdr_V_TVALID(usr_tx_hdr_V_TVALID),          // output wire usr_tx_hdr_V_TVALID
	.usr_tx_hdr_V_TREADY(usr_tx_hdr_V_TREADY),          // input wire usr_tx_hdr_V_TREADY
	.usr_tx_hdr_V_TDATA(usr_tx_hdr_V_TDATA)            // output wire [111 : 0] usr_tx_hdr_V_TDATA
);

always #CLK_PERIOD clk <= ~clk;

// initialize transaction
initial begin

	enable_send <= 1'b0;
	enable_rst <= 1'b0;
	enable_setconn <= 1'b0;
	enable_retrans <= 1'b0;

	$display("initilization start");

	tx_agent = new("tx pld agent",ex_host.axi4stream_vip_tx_payload.inst.IF);
	tx_hdr_agent = new("tx hdr agent",ex_host.axi4stream_vip_tx_hdr.inst.IF);
	tx_set_agent = new("set conn agent",ex_host.axi4stream_vip_tx_setconn.inst.IF);
	slv_axi_agent = new("slv mem agent",relnet_inst.axi_vip.inst.IF);

	tx_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	tx_hdr_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);
	tx_set_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);

	wr_trans = tx_agent.driver.create_transaction("tx pld trans");
	wr_hdr_trans = tx_hdr_agent.driver.create_transaction("tx hdr trans");
	wr_set_trans = tx_set_agent.driver.create_transaction("set conn trans");

	tx_agent.start_master();
	tx_hdr_agent.start_master();
	tx_set_agent.start_master();
	slv_axi_agent.start_slave();
	
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
	mst_ip <= {8'd192, 8'd168, 8'd1,   8'd2};
	slv_ip <= {8'd192, 8'd168, 8'd1,   8'd128};
	enable_setconn <= 1'b1;
end

bit [31:0] test_seq [7] = {1, 2, 3, 4, 5, 6, 7};
pkt_type_t typ = pkt_data;

initial begin
	wait(enable_setconn == 1'b1);

	/*
	 * somehow you have to do this otherwise it will output X
	 */
	assert(wr_trans.randomize());
	assert(wr_hdr_trans.randomize());
	assert(wr_set_trans.randomize());

	// set connection state in master side: slot 20
	set_req = '{8'd20, 8'd4};  // {10'd20(slot id), 6'd1(type GBN_SOC2FPGA_SET_TYPE_OPEN)}

	wr_set_trans.set_data(set_req);
	tx_set_agent.driver.send(wr_set_trans);

	#CLK_PERIOD;
	// set connection state in slave side
	src_ip = {<<8{mst_ip}};
	dest_ip = {<<8{slv_ip}};
	src_port = {<<8{16'd20}};  // source port acts as src slot id
	dest_port = {<<8{16'd0}}; // destination port acts as dest slot id
	length = {<<8{16'd24}};

	session_id = {<<8{8'd20, 8'd0, 8'd0}};  // {10'd20(src slot), 10'd0(dest slot), 4'd0}

	wr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	#CLK_PERIOD;
	data = {<<8{session_id, 32'd0, typ}};
	wr_trans.set_data(data);
	wr_trans.set_last(1'b0);
	$display("send SYN udp head");
	tx_hdr_agent.driver.send(wr_hdr_trans);
	$display("send SYN udp data");
	tx_agent.driver.send(wr_trans);

	$display("send SYN udp data");
	{>>{data}} = 64'h0202020202020202;
	wr_trans.set_data(data);
	wr_trans.set_last(1'b1);
	tx_agent.driver.send(wr_trans);

	#200;
	enable_send <= 1'b1;
end

// send gbn header and udp header
initial begin
	wait(enable_send == 1'b1);

	src_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd2}};
	dest_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd128}};
	src_port = {<<8{16'd1000}};
	dest_port = {<<8{16'd1234}};
	length = {<<8{16'd32}};	// 2*64bit

	session_id = {<<8{8'd20, 8'd40, 8'd0}};  // {10'd20(src slot), 10'd10(dest slot), 4'd0}

	wr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	for (int i = 0; i < $size(test_seq); i++) begin
		{>>{seqnum}} <= test_seq[i];

		#CLK_PERIOD;
		data = {<<8{session_id, seqnum, typ}};
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

	#200;
	enable_retrans <= 1'b1;
end

initial begin
	wait(enable_retrans == 1'b1);

	src_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd2}};
	dest_ip = {<<8{8'd192, 8'd168, 8'd1,   8'd128}};
	src_port = {<<8{16'd1000}};
	dest_port = {<<8{16'd1234}};
	length = {<<8{16'd16}};	// 2*64bit

	session_id = {<<8{8'd20, 8'd40, 8'd0}};  // {10'd20(src slot), 10'd10(dest slot), 4'd0}

	wr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	#CLK_PERIOD;
	data = {<<8{session_id, 32'd3, pkt_nack}};
	wr_trans.set_data(data);
	wr_trans.set_last(1'b1);
	$display("send NACK udp head");
	tx_hdr_agent.driver.send(wr_hdr_trans);
	$display("send NACK gbn head");
	tx_agent.driver.send(wr_trans);

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

endmodule
