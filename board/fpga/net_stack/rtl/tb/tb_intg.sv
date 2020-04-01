/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

`timescale 1ns / 1ps

import axi4stream_vip_pkg::*;
import ex_sim_axi4stream_vip_tx_payload_0_pkg::*;
import ex_sim_axi4stream_vip_tx_hdr_0_pkg::*;
import ex_sim_axi4stream_vip_tx_setconn_0_pkg::*;

module testbench_netstack_2;

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

logic [7:0] mst_ip[4];
logic [7:0] slv_ip[4];

logic [7:0] src_ip[4];
logic [7:0] dest_ip[4];
logic [7:0] src_port[2];
logic [7:0] dest_port[2];
logic [7:0] length[2];

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

wire [15:0] setconn_mst_axis_tdata;
wire setconn_mst_axis_tready;
wire setconn_mst_axis_tvalid;
wire [15:0] setconn_slv_axis_tdata;
wire setconn_slv_axis_tready;
wire setconn_slv_axis_tvalid;

wire [111:0] tx_udp_hdr_data;
wire tx_udp_hdr_valid;
wire tx_udp_hdr_ready;
wire [63:0] tx_udp_payload_axis_tdata;
wire [7:0] tx_udp_payload_axis_tkeep;
wire tx_udp_payload_axis_tvalid;
wire tx_udp_payload_axis_tready;
wire tx_udp_payload_axis_tlast;
wire tx_udp_payload_axis_tuser;

wire [111:0] rx_udp_hdr_data;
wire rx_udp_hdr_valid;
wire rx_udp_hdr_ready;
wire [63:0] rx_udp_payload_axis_tdata;
wire [7:0] rx_udp_payload_axis_tkeep;
wire rx_udp_payload_axis_tvalid;
wire rx_udp_payload_axis_tready;
wire rx_udp_payload_axis_tlast;
wire rx_udp_payload_axis_tuser;

wire [31:0] rx_udp_ip_source_ip;
wire [31:0] rx_udp_ip_dest_ip;
wire [15:0] rx_udp_source_port;
wire [15:0] rx_udp_dest_port;
wire [15:0] rx_udp_length;

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

wire [31:0] slv_rx_udp_ip_source_ip;
wire [31:0] slv_rx_udp_ip_dest_ip;
wire [15:0] slv_rx_udp_source_port;
wire [15:0] slv_rx_udp_dest_port;
wire [15:0] slv_rx_udp_length;

ex_sim ex_host (
	.M_AXIS_hdr_tdata(tx_udp_hdr_data),
	.M_AXIS_hdr_tready(tx_udp_hdr_ready),
	.M_AXIS_hdr_tvalid(tx_udp_hdr_valid),
	.M_AXIS_payload_tdata(tx_udp_payload_axis_tdata),
	.M_AXIS_payload_tkeep(tx_udp_payload_axis_tkeep),
	.M_AXIS_payload_tlast(tx_udp_payload_axis_tlast),
	.M_AXIS_payload_tready(tx_udp_payload_axis_tready),
	.M_AXIS_payload_tvalid(tx_udp_payload_axis_tvalid),
	.M_AXIS_setconn_tdata(setconn_mst_axis_tdata),
	.M_AXIS_setconn_tready(setconn_mst_axis_tready),
	.M_AXIS_setconn_tvalid(setconn_mst_axis_tvalid),
	.aclk(clk),
	.aresetn(~rst)
);

assign tx_udp_payload_axis_tuser = 1'b0;
assign rx_udp_hdr_ready = 1'b1;
assign rx_udp_payload_axis_tready = 1'b1;

assign {
	rx_udp_length,
	rx_udp_dest_port,
	rx_udp_source_port,
	rx_udp_ip_dest_ip,
	rx_udp_ip_source_ip
} = rx_udp_hdr_data;

assign {
	slv_rx_udp_length,
	slv_rx_udp_dest_port,
	slv_rx_udp_source_port,
	slv_rx_udp_ip_dest_ip,
	slv_rx_udp_ip_source_ip
} = slv_rx_udp_hdr_data;

fpga_core #(
	.INTEGRATION_MODE(1)
)
relnet_core_mst (
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
	.s_usr_hdr_data(tx_udp_hdr_data),
	.s_usr_hdr_valid(tx_udp_hdr_valid),
	.s_usr_hdr_ready(tx_udp_hdr_ready),
	.s_usr_payload_axis_tdata(tx_udp_payload_axis_tdata),
	.s_usr_payload_axis_tvalid(tx_udp_payload_axis_tvalid),
	.s_usr_payload_axis_tready(tx_udp_payload_axis_tready),
	.s_usr_payload_axis_tlast(tx_udp_payload_axis_tlast),
	.s_usr_payload_axis_tkeep(tx_udp_payload_axis_tkeep),
	.s_usr_payload_axis_tuser(tx_udp_payload_axis_tuser),

	// onboard pipeline output
	.m_usr_hdr_data(rx_udp_hdr_data),
	.m_usr_hdr_valid(rx_udp_hdr_valid),
	.m_usr_hdr_ready(rx_udp_hdr_ready),
	.m_usr_payload_axis_tdata(rx_udp_payload_axis_tdata),
	.m_usr_payload_axis_tvalid(rx_udp_payload_axis_tvalid),
	.m_usr_payload_axis_tready(rx_udp_payload_axis_tready),
	.m_usr_payload_axis_tlast(rx_udp_payload_axis_tlast),
	.m_usr_payload_axis_tkeep(rx_udp_payload_axis_tkeep),
	.m_usr_payload_axis_tuser(rx_udp_payload_axis_tuser),

	// set connection input
	.s_setconn_axis_tdata(setconn_mst_axis_tdata),
	.s_setconn_axis_tvalid(setconn_mst_axis_tvalid),
	.s_setconn_axis_tready(setconn_mst_axis_tready),

	// identity info
	.local_ip({>>{mst_ip}})
);

fpga_core #(
	.INTEGRATION_MODE(1)
)
relnet_core_slv (
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
	.s_usr_hdr_data(slv_tx_udp_hdr_data),
	.s_usr_hdr_valid(slv_tx_udp_hdr_valid),
	.s_usr_hdr_ready(slv_tx_udp_hdr_ready),
	.s_usr_payload_axis_tdata(slv_tx_udp_payload_axis_tdata),
	.s_usr_payload_axis_tvalid(slv_tx_udp_payload_axis_tvalid),
	.s_usr_payload_axis_tready(slv_tx_udp_payload_axis_tready),
	.s_usr_payload_axis_tlast(slv_tx_udp_payload_axis_tlast),
	.s_usr_payload_axis_tkeep(slv_tx_udp_payload_axis_tkeep),
	.s_usr_payload_axis_tuser(slv_tx_udp_payload_axis_tuser),

	// onboard pipeline output
	.m_usr_hdr_data(slv_rx_udp_hdr_data),
	.m_usr_hdr_valid(slv_rx_udp_hdr_valid),
	.m_usr_hdr_ready(slv_rx_udp_hdr_ready),
	.m_usr_payload_axis_tdata(slv_rx_udp_payload_axis_tdata),
	.m_usr_payload_axis_tvalid(slv_rx_udp_payload_axis_tvalid),
	.m_usr_payload_axis_tready(slv_rx_udp_payload_axis_tready),
	.m_usr_payload_axis_tlast(slv_rx_udp_payload_axis_tlast),
	.m_usr_payload_axis_tkeep(slv_rx_udp_payload_axis_tkeep),
	.m_usr_payload_axis_tuser(slv_rx_udp_payload_axis_tuser),

	// set connection input
	.s_setconn_axis_tdata(setconn_slv_axis_tdata),
	.s_setconn_axis_tvalid(setconn_slv_axis_tvalid),
	.s_setconn_axis_tready(setconn_slv_axis_tready),

	// identity info
	.local_ip({>>{slv_ip}})
);

dummy_setup_inst
your_instance_name (
	.ap_clk(clk),                                    // input wire ap_clk
	.ap_rst_n(~rst),                                // input wire ap_rst_n
	.usr_rx_payload_TVALID(slv_rx_udp_payload_axis_tvalid),      // input wire usr_rx_payload_TVALID
	.usr_rx_payload_TREADY(slv_rx_udp_payload_axis_tready),      // output wire usr_rx_payload_TREADY
	.usr_rx_payload_TDATA(slv_rx_udp_payload_axis_tdata),        // input wire [63 : 0] usr_rx_payload_TDATA
	.usr_rx_payload_TUSER(slv_rx_udp_payload_axis_tuser),        // input wire [0 : 0] usr_rx_payload_TUSER
	.usr_rx_payload_TLAST(slv_rx_udp_payload_axis_tlast),        // input wire [0 : 0] usr_rx_payload_TLAST
	.usr_rx_payload_TKEEP(slv_rx_udp_payload_axis_tkeep),        // input wire [7 : 0] usr_rx_payload_TKEEP
	.usr_rx_hdr_V_TVALID(slv_rx_udp_hdr_valid),          // input wire usr_rx_hdr_V_TVALID
	.usr_rx_hdr_V_TREADY(slv_rx_udp_hdr_ready),          // output wire usr_rx_hdr_V_TREADY
	.usr_rx_hdr_V_TDATA(slv_rx_udp_hdr_data),            // input wire [111 : 0] usr_rx_hdr_V_TDATA
	.conn_setup_req_V_TVALID(setconn_slv_axis_tvalid),  // output wire conn_setup_req_V_TVALID
	.conn_setup_req_V_TREADY(setconn_slv_axis_tready),  // input wire conn_setup_req_V_TREADY
	.conn_setup_req_V_TDATA(setconn_slv_axis_tdata),    // output wire [15 : 0] conn_setup_req_V_TDATA
	.usr_tx_payload_TVALID(slv_tx_udp_payload_axis_tvalid),      // output wire usr_tx_payload_TVALID
	.usr_tx_payload_TREADY(slv_tx_udp_payload_axis_tready),      // input wire usr_tx_payload_TREADY
	.usr_tx_payload_TDATA(slv_tx_udp_payload_axis_tdata),        // output wire [63 : 0] usr_tx_payload_TDATA
	.usr_tx_payload_TUSER(slv_tx_udp_payload_axis_tuser),        // output wire [0 : 0] usr_tx_payload_TUSER
	.usr_tx_payload_TLAST(slv_tx_udp_payload_axis_tlast),        // output wire [0 : 0] usr_tx_payload_TLAST
	.usr_tx_payload_TKEEP(slv_tx_udp_payload_axis_tkeep),        // output wire [7 : 0] usr_tx_payload_TKEEP
	.usr_tx_hdr_V_TVALID(slv_tx_udp_hdr_valid),          // output wire usr_tx_hdr_V_TVALID
	.usr_tx_hdr_V_TREADY(slv_tx_udp_hdr_ready),          // input wire usr_tx_hdr_V_TREADY
	.usr_tx_hdr_V_TDATA(slv_tx_udp_hdr_data)             // output wire [111 : 0] usr_tx_hdr_V_TDATA
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

	#20
	rst <= 1'b1;

	#160
	rst <= 1'b0;

	#20
	mst_ip <= {8'd192, 8'd168, 8'd1,   8'd2};
	slv_ip <= {8'd192, 8'd168, 8'd1,   8'd128};
	enable_setconn <= 1'b1;
end

initial begin
	wait(enable_setconn == 1'b1);

	/*
	 * somehow you have to do this otherwise it will output X
	 */
	assert(wr_trans.randomize());
	assert(wr_hdr_trans.randomize());
	assert(wr_set_trans.randomize());

	// set connection state in master side: slot 20
	set_req = '{8'd20, 8'd4};  // {10'd20(slot id), 6'd1(type set_type_open)}

	wr_set_trans.set_data(set_req);
	tx_set_agent.driver.send(wr_set_trans);

	#CLK_PERIOD;
	// set connection state in slave side
	src_ip = {<<8{mst_ip}};
	dest_ip = {<<8{slv_ip}};
	src_port = {<<8{16'd20}};  // source port acts as src slot id
	dest_port = {<<8{16'd0}}; // destination port acts as dest slot id
	length = {<<8{16'd16}};

	wr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	#CLK_PERIOD;
	$display("send SYN udp head");
	tx_hdr_agent.driver.send(wr_hdr_trans);
	$display("send SYN udp data");
	wr_trans.set_data({8'd0, 8'd0, 8'd0, 8'd0, 8'd7, 8'd0, 8'd0, 8'd0});  // lego header open session
	wr_trans.set_last(1'b0);
	tx_agent.driver.send(wr_trans);

	data = {<<8{64'd10}};
	wr_trans.set_data(data);
	wr_trans.set_last(1'b1);
	tx_agent.driver.send(wr_trans);

	wait(rx_udp_hdr_valid == 1'b1);
	enable_send <= 1'b1;
end

initial begin
	wait(enable_send == 1'b1);

	src_ip = {<<8{mst_ip}};
	dest_ip = {<<8{slv_ip}};
	src_port = {<<8{16'd20}};  // source port acts as src slot id
	dest_port = {<<8{16'd10}}; // destination port acts as dest slot id
	length = {<<8{16'd24}};	// 2*64bit

	wr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	for (int i = 0; i < 10; i++) begin

		#CLK_PERIOD;
		$display("send udp head");
		tx_hdr_agent.driver.send(wr_hdr_trans);

		$display("send udp data");
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

	src_ip = {<<8{mst_ip}};
	dest_ip = {<<8{slv_ip}};
	src_port = {<<8{16'd0}};  // source port acts as src slot id
	dest_port = {<<8{16'd10}}; // destination port acts as dest slot id
	length = {<<8{16'd16}};	// 2*64bit

	wr_hdr_trans.set_data({src_ip, dest_ip, src_port, dest_port, length});

	#CLK_PERIOD;
	$display("send FIN udp head");
	tx_hdr_agent.driver.send(wr_hdr_trans);
	$display("send FIN udp data");
	wr_trans.set_data({8'd0, 8'd0, 8'd0, 8'd0, 8'd8, 8'd0, 8'd0, 8'd0});  // lego header open session
	wr_trans.set_last(1'b0);
	tx_agent.driver.send(wr_trans);

	data = {<<8{64'd10}};
	wr_trans.set_data(data);
	wr_trans.set_last(1'b1);
	tx_agent.driver.send(wr_trans);

end

always @(posedge clk) begin
	if (rx_udp_hdr_valid && rx_udp_hdr_ready) begin
		$display("master receive udp header: %h:%d -> %h:%d",
			rx_udp_ip_source_ip,
			rx_udp_source_port,
			rx_udp_ip_dest_ip,
			rx_udp_dest_port);
	end
	if (rx_udp_payload_axis_tvalid && rx_udp_payload_axis_tready) begin
		$display("master receive data: %x", rx_udp_payload_axis_tdata);
	end
end

always @(posedge clk) begin
	if (slv_rx_udp_hdr_valid && slv_rx_udp_hdr_ready) begin
		$display("slave deliever udp header: %h:%d -> %h:%d",
			slv_rx_udp_ip_source_ip,
			slv_rx_udp_source_port,
			slv_rx_udp_ip_dest_ip,
			slv_rx_udp_dest_port);
	end
	if (slv_rx_udp_payload_axis_tvalid && slv_rx_udp_payload_axis_tready) begin
		$display("slave deliever data: %x", slv_rx_udp_payload_axis_tdata);
	end
end

endmodule // testbench_netstack
