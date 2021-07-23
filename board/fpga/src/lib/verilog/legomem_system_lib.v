/*

Copyright (c) 2014-2018 Alex Forencich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

// Language: Verilog 2001

`timescale 1ns / 1ps

/*
 * AXI4-Stream bus width adapter
 */
module axis_adapter #
(
    // Width of input AXI stream interface in bits
    parameter S_DATA_WIDTH = 8,
    // Propagate tkeep signal on input interface
    // If disabled, tkeep assumed to be 1'b1
    parameter S_KEEP_ENABLE = (S_DATA_WIDTH>8),
    // tkeep signal width (words per cycle) on input interface
    parameter S_KEEP_WIDTH = (S_DATA_WIDTH/8),
    // Width of output AXI stream interface in bits
    parameter M_DATA_WIDTH = 8,
    // Propagate tkeep signal on output interface
    // If disabled, tkeep assumed to be 1'b1
    parameter M_KEEP_ENABLE = (M_DATA_WIDTH>8),
    // tkeep signal width (words per cycle) on output interface
    parameter M_KEEP_WIDTH = (M_DATA_WIDTH/8),
    // Propagate tid signal
    parameter ID_ENABLE = 0,
    // tid signal width
    parameter ID_WIDTH = 8,
    // Propagate tdest signal
    parameter DEST_ENABLE = 0,
    // tdest signal width
    parameter DEST_WIDTH = 8,
    // Propagate tuser signal
    parameter USER_ENABLE = 1,
    // tuser signal width
    parameter USER_WIDTH = 1
)
(
    input  wire                     clk,
    input  wire                     rstn,

    /*
     * AXI input
     */
    input  wire [S_DATA_WIDTH-1:0]  s_axis_tdata,
    input  wire [S_KEEP_WIDTH-1:0]  s_axis_tkeep,
    input  wire                     s_axis_tvalid,
    output wire                     s_axis_tready,
    input  wire                     s_axis_tlast,
    input  wire [ID_WIDTH-1:0]      s_axis_tid,
    input  wire [DEST_WIDTH-1:0]    s_axis_tdest,
    input  wire [USER_WIDTH-1:0]    s_axis_tuser,

    /*
     * AXI output
     */
    output wire [M_DATA_WIDTH-1:0]  m_axis_tdata,
    output wire [M_KEEP_WIDTH-1:0]  m_axis_tkeep,
    output wire                     m_axis_tvalid,
    input  wire                     m_axis_tready,
    output wire                     m_axis_tlast,
    output wire [ID_WIDTH-1:0]      m_axis_tid,
    output wire [DEST_WIDTH-1:0]    m_axis_tdest,
    output wire [USER_WIDTH-1:0]    m_axis_tuser
);

wire rst = ~rstn;

// force keep width to 1 when disabled
parameter S_KEEP_WIDTH_INT = S_KEEP_ENABLE ? S_KEEP_WIDTH : 1;
parameter M_KEEP_WIDTH_INT = M_KEEP_ENABLE ? M_KEEP_WIDTH : 1;

// bus word sizes (must be identical)
parameter S_DATA_WORD_SIZE = S_DATA_WIDTH / S_KEEP_WIDTH_INT;
parameter M_DATA_WORD_SIZE = M_DATA_WIDTH / M_KEEP_WIDTH_INT;
// output bus is wider
parameter EXPAND_BUS = M_KEEP_WIDTH_INT > S_KEEP_WIDTH_INT;
// total data and keep widths
parameter DATA_WIDTH = EXPAND_BUS ? M_DATA_WIDTH : S_DATA_WIDTH;
parameter KEEP_WIDTH = EXPAND_BUS ? M_KEEP_WIDTH_INT : S_KEEP_WIDTH_INT;
// required number of segments in wider bus
parameter SEGMENT_COUNT = EXPAND_BUS ? (M_KEEP_WIDTH_INT / S_KEEP_WIDTH_INT) : (S_KEEP_WIDTH_INT / M_KEEP_WIDTH_INT);
parameter SEGMENT_COUNT_WIDTH = SEGMENT_COUNT == 1 ? 1 : $clog2(SEGMENT_COUNT);
// data width and keep width per segment
parameter SEGMENT_DATA_WIDTH = DATA_WIDTH / SEGMENT_COUNT;
parameter SEGMENT_KEEP_WIDTH = KEEP_WIDTH / SEGMENT_COUNT;

// bus width assertions
// initial begin
//     if (S_DATA_WORD_SIZE * S_KEEP_WIDTH_INT != S_DATA_WIDTH) begin
//         $error("Error: input data width not evenly divisble (instance %m)");
//         $finish;
//     end
//
//     if (M_DATA_WORD_SIZE * M_KEEP_WIDTH_INT != M_DATA_WIDTH) begin
//         $error("Error: output data width not evenly divisble (instance %m)");
//         $finish;
//     end
//
//     if (S_DATA_WORD_SIZE != M_DATA_WORD_SIZE) begin
//         $error("Error: word size mismatch (instance %m)");
//         $finish;
//     end
// end

// state register
localparam [2:0]
    STATE_IDLE = 3'd0,
    STATE_TRANSFER_IN = 3'd1,
    STATE_TRANSFER_OUT = 3'd2;

reg [2:0] state_reg = STATE_IDLE, state_next;

reg [SEGMENT_COUNT_WIDTH-1:0] segment_count_reg = 0, segment_count_next;

reg last_segment;

reg [DATA_WIDTH-1:0] temp_tdata_reg = {DATA_WIDTH{1'b0}}, temp_tdata_next;
reg [KEEP_WIDTH-1:0] temp_tkeep_reg = {KEEP_WIDTH{1'b0}}, temp_tkeep_next;
reg                  temp_tlast_reg = 1'b0, temp_tlast_next;
reg [ID_WIDTH-1:0]   temp_tid_reg   = {ID_WIDTH{1'b0}}, temp_tid_next;
reg [DEST_WIDTH-1:0] temp_tdest_reg = {DEST_WIDTH{1'b0}}, temp_tdest_next;
reg [USER_WIDTH-1:0] temp_tuser_reg = {USER_WIDTH{1'b0}}, temp_tuser_next;

// internal datapath
reg  [M_DATA_WIDTH-1:0] m_axis_tdata_int;
reg  [M_KEEP_WIDTH-1:0] m_axis_tkeep_int;
reg                     m_axis_tvalid_int;
reg                     m_axis_tready_int_reg = 1'b0;
reg                     m_axis_tlast_int;
reg  [ID_WIDTH-1:0]     m_axis_tid_int;
reg  [DEST_WIDTH-1:0]   m_axis_tdest_int;
reg  [USER_WIDTH-1:0]   m_axis_tuser_int;
wire                    m_axis_tready_int_early;

reg s_axis_tready_reg = 1'b0, s_axis_tready_next;

assign s_axis_tready = s_axis_tready_reg;

always @* begin
    state_next = STATE_IDLE;

    segment_count_next = segment_count_reg;

    last_segment = 0;

    temp_tdata_next = temp_tdata_reg;
    temp_tkeep_next = temp_tkeep_reg;
    temp_tlast_next = temp_tlast_reg;
    temp_tid_next   = temp_tid_reg;
    temp_tdest_next = temp_tdest_reg;
    temp_tuser_next = temp_tuser_reg;

    if (EXPAND_BUS) begin
        m_axis_tdata_int  = temp_tdata_reg;
        m_axis_tkeep_int  = temp_tkeep_reg;
        m_axis_tlast_int  = temp_tlast_reg;
    end else begin
        m_axis_tdata_int  = {M_DATA_WIDTH{1'b0}};
        m_axis_tkeep_int  = {M_KEEP_WIDTH{1'b0}};
        m_axis_tlast_int  = 1'b0;
    end
    m_axis_tvalid_int = 1'b0;
    m_axis_tid_int    = temp_tid_reg;
    m_axis_tdest_int  = temp_tdest_reg;
    m_axis_tuser_int  = temp_tuser_reg;

    s_axis_tready_next = 1'b0;

    case (state_reg)
        STATE_IDLE: begin
            // idle state - no data in registers
            if (SEGMENT_COUNT == 1) begin
                // output and input same width - just act like a register

                // accept data next cycle if output register ready next cycle
                s_axis_tready_next = m_axis_tready_int_early;

                // transfer through
                m_axis_tdata_int  = s_axis_tdata;
                m_axis_tkeep_int  = S_KEEP_ENABLE ? s_axis_tkeep : 1'b1;
                m_axis_tvalid_int = s_axis_tvalid;
                m_axis_tlast_int  = s_axis_tlast;
                m_axis_tid_int    = s_axis_tid;
                m_axis_tdest_int  = s_axis_tdest;
                m_axis_tuser_int  = s_axis_tuser;

                state_next = STATE_IDLE;
            end else if (EXPAND_BUS) begin
                // output bus is wider

                // accept new data
                s_axis_tready_next = 1'b1;

                if (s_axis_tready && s_axis_tvalid) begin
                    // word transfer in - store it in data register

                    // pass complete input word, zero-extended to temp register
                    temp_tdata_next = s_axis_tdata;
                    temp_tkeep_next = S_KEEP_ENABLE ? s_axis_tkeep : 1'b1;
                    temp_tlast_next = s_axis_tlast;
                    temp_tid_next   = s_axis_tid;
                    temp_tdest_next = s_axis_tdest;
                    temp_tuser_next = s_axis_tuser;

                    // first input segment complete
                    segment_count_next = 1;

                    if (s_axis_tlast) begin
                        // got last signal on first segment, so output it
                        s_axis_tready_next = 1'b0;
                        state_next = STATE_TRANSFER_OUT;
                    end else begin
                        // otherwise, transfer in the rest of the words
                        s_axis_tready_next = 1'b1;
                        state_next = STATE_TRANSFER_IN;
                    end
                end else begin
                    state_next = STATE_IDLE;
                end
            end else begin
                // output bus is narrower

                // accept new data
                s_axis_tready_next = 1'b1;

                if (s_axis_tready && s_axis_tvalid) begin
                    // word transfer in - store it in data register
                    segment_count_next = 0;

                    // is this the last segment?
                    if (SEGMENT_COUNT == 1) begin
                        // last segment by counter value
                        last_segment = 1'b1;
                    end else if (S_KEEP_ENABLE && s_axis_tkeep[SEGMENT_KEEP_WIDTH-1:0] != {SEGMENT_KEEP_WIDTH{1'b1}}) begin
                        // last segment by tkeep fall in current segment
                        last_segment = 1'b1;
                    end else if (S_KEEP_ENABLE && s_axis_tkeep[(SEGMENT_KEEP_WIDTH*2)-1:SEGMENT_KEEP_WIDTH] == {SEGMENT_KEEP_WIDTH{1'b0}}) begin
                        // last segment by tkeep fall at end of current segment
                        last_segment = 1'b1;
                    end else begin
                        last_segment = 1'b0;
                    end

                    // pass complete input word, zero-extended to temp register
                    temp_tdata_next = s_axis_tdata;
                    temp_tkeep_next = S_KEEP_ENABLE ? s_axis_tkeep : 1'b1;
                    temp_tlast_next = s_axis_tlast;
                    temp_tid_next   = s_axis_tid;
                    temp_tdest_next = s_axis_tdest;
                    temp_tuser_next = s_axis_tuser;

                    // short-circuit and get first word out the door
                    m_axis_tdata_int  = s_axis_tdata[SEGMENT_DATA_WIDTH-1:0];
                    m_axis_tkeep_int  = s_axis_tkeep[SEGMENT_KEEP_WIDTH-1:0];
                    m_axis_tvalid_int = 1'b1;
                    m_axis_tlast_int  = s_axis_tlast & last_segment;
                    m_axis_tid_int    = s_axis_tid;
                    m_axis_tdest_int  = s_axis_tdest;
                    m_axis_tuser_int  = s_axis_tuser;

                    if (m_axis_tready_int_reg) begin
                        // if output register is ready for first word, then move on to the next one
                        segment_count_next = 1;
                    end

                    if (!last_segment || !m_axis_tready_int_reg) begin
                        // continue outputting words
                        s_axis_tready_next = 1'b0;
                        state_next = STATE_TRANSFER_OUT;
                    end else begin
                        state_next = STATE_IDLE;
                    end
                end else begin
                    state_next = STATE_IDLE;
                end
            end
        end
        STATE_TRANSFER_IN: begin
            // transfer word to temp registers
            // only used when output is wider

            // accept new data
            s_axis_tready_next = 1'b1;

            if (s_axis_tready && s_axis_tvalid) begin
                // word transfer in - store in data register

                temp_tdata_next[segment_count_reg*SEGMENT_DATA_WIDTH +: SEGMENT_DATA_WIDTH] = s_axis_tdata;
                temp_tkeep_next[segment_count_reg*SEGMENT_KEEP_WIDTH +: SEGMENT_KEEP_WIDTH] = S_KEEP_ENABLE ? s_axis_tkeep : 1'b1;
                temp_tlast_next = s_axis_tlast;
                temp_tid_next   = s_axis_tid;
                temp_tdest_next = s_axis_tdest;
                temp_tuser_next = s_axis_tuser;

                segment_count_next = segment_count_reg + 1;

                if ((segment_count_reg == SEGMENT_COUNT-1) || s_axis_tlast) begin
                    // terminated by counter or tlast signal, output complete word
                    // read input word next cycle if output will be ready
                    s_axis_tready_next = m_axis_tready_int_early;
                    state_next = STATE_TRANSFER_OUT;
                end else begin
                    // more words to read
                    s_axis_tready_next = 1'b1;
                    state_next = STATE_TRANSFER_IN;
                end
            end else begin
                state_next = STATE_TRANSFER_IN;
            end
        end
        STATE_TRANSFER_OUT: begin
            // transfer word to output registers

            if (EXPAND_BUS) begin
                // output bus is wider

                // do not accept new data
                s_axis_tready_next = 1'b0;

                // single-cycle output of entire stored word (output wider)
                m_axis_tdata_int  = temp_tdata_reg;
                m_axis_tkeep_int  = temp_tkeep_reg;
                m_axis_tvalid_int = 1'b1;
                m_axis_tlast_int  = temp_tlast_reg;
                m_axis_tid_int    = temp_tid_reg;
                m_axis_tdest_int  = temp_tdest_reg;
                m_axis_tuser_int  = temp_tuser_reg;

                if (m_axis_tready_int_reg) begin
                    // word transfer out

                    if (s_axis_tready && s_axis_tvalid) begin
                        // word transfer in

                        // pass complete input word, zero-extended to temp register
                        temp_tdata_next = s_axis_tdata;
                        temp_tkeep_next = S_KEEP_ENABLE ? s_axis_tkeep : 1'b1;
                        temp_tlast_next = s_axis_tlast;
                        temp_tid_next   = s_axis_tid;
                        temp_tdest_next = s_axis_tdest;
                        temp_tuser_next = s_axis_tuser;

                        // first input segment complete
                        segment_count_next = 1;

                        if (s_axis_tlast) begin
                            // got last signal on first segment, so output it
                            s_axis_tready_next = 1'b0;
                            state_next = STATE_TRANSFER_OUT;
                        end else begin
                            // otherwise, transfer in the rest of the words
                            s_axis_tready_next = 1'b1;
                            state_next = STATE_TRANSFER_IN;
                        end
                    end else begin
                        s_axis_tready_next = 1'b1;
                        state_next = STATE_IDLE;
                    end
                end else begin
                    state_next = STATE_TRANSFER_OUT;
                end
            end else begin
                // output bus is narrower

                // do not accept new data
                s_axis_tready_next = 1'b0;

                // is this the last segment?
                if (segment_count_reg == SEGMENT_COUNT-1) begin
                    // last segment by counter value
                    last_segment = 1'b1;
                end else if (temp_tkeep_reg[segment_count_reg*SEGMENT_KEEP_WIDTH +: SEGMENT_KEEP_WIDTH] != {SEGMENT_KEEP_WIDTH{1'b1}}) begin
                    // last segment by tkeep fall in current segment
                    last_segment = 1'b1;
                end else if (temp_tkeep_reg[(segment_count_reg+1)*SEGMENT_KEEP_WIDTH +: SEGMENT_KEEP_WIDTH] == {SEGMENT_KEEP_WIDTH{1'b0}}) begin
                    // last segment by tkeep fall at end of current segment
                    last_segment = 1'b1;
                end else begin
                    last_segment = 1'b0;
                end

                // output current part of stored word (output narrower)
                m_axis_tdata_int  = temp_tdata_reg[segment_count_reg*SEGMENT_DATA_WIDTH +: SEGMENT_DATA_WIDTH];
                m_axis_tkeep_int  = temp_tkeep_reg[segment_count_reg*SEGMENT_KEEP_WIDTH +: SEGMENT_KEEP_WIDTH];
                m_axis_tvalid_int = 1'b1;
                m_axis_tlast_int  = temp_tlast_reg && last_segment;
                m_axis_tid_int    = temp_tid_reg;
                m_axis_tdest_int  = temp_tdest_reg;
                m_axis_tuser_int  = temp_tuser_reg;

                if (m_axis_tready_int_reg) begin
                    // word transfer out

                    segment_count_next = segment_count_reg + 1;

                    if (last_segment) begin
                        // terminated by counter or tlast signal

                        s_axis_tready_next = 1'b1;
                        state_next = STATE_IDLE;
                    end else begin
                        // more words to write
                        state_next = STATE_TRANSFER_OUT;
                    end
                end else begin
                    state_next = STATE_TRANSFER_OUT;
                end
            end
        end
    endcase
end

always @(posedge clk) begin
    if (rst) begin
        state_reg <= STATE_IDLE;
        s_axis_tready_reg <= 1'b0;
    end else begin
        state_reg <= state_next;

        s_axis_tready_reg <= s_axis_tready_next;
    end

    segment_count_reg <= segment_count_next;

    temp_tdata_reg <= temp_tdata_next;
    temp_tkeep_reg <= temp_tkeep_next;
    temp_tlast_reg <= temp_tlast_next;
    temp_tid_reg   <= temp_tid_next;
    temp_tdest_reg <= temp_tdest_next;
    temp_tuser_reg <= temp_tuser_next;
end

// output datapath logic
reg [M_DATA_WIDTH-1:0] m_axis_tdata_reg  = {M_DATA_WIDTH{1'b0}};
reg [M_KEEP_WIDTH-1:0] m_axis_tkeep_reg  = {M_KEEP_WIDTH{1'b0}};
reg                    m_axis_tvalid_reg = 1'b0, m_axis_tvalid_next;
reg                    m_axis_tlast_reg  = 1'b0;
reg [ID_WIDTH-1:0]     m_axis_tid_reg    = {ID_WIDTH{1'b0}};
reg [DEST_WIDTH-1:0]   m_axis_tdest_reg  = {DEST_WIDTH{1'b0}};
reg [USER_WIDTH-1:0]   m_axis_tuser_reg  = {USER_WIDTH{1'b0}};

reg [M_DATA_WIDTH-1:0] temp_m_axis_tdata_reg  = {M_DATA_WIDTH{1'b0}};
reg [M_KEEP_WIDTH-1:0] temp_m_axis_tkeep_reg  = {M_KEEP_WIDTH{1'b0}};
reg                    temp_m_axis_tvalid_reg = 1'b0, temp_m_axis_tvalid_next;
reg                    temp_m_axis_tlast_reg  = 1'b0;
reg [ID_WIDTH-1:0]     temp_m_axis_tid_reg    = {ID_WIDTH{1'b0}};
reg [DEST_WIDTH-1:0]   temp_m_axis_tdest_reg  = {DEST_WIDTH{1'b0}};
reg [USER_WIDTH-1:0]   temp_m_axis_tuser_reg  = {USER_WIDTH{1'b0}};

// datapath control
reg store_axis_int_to_output;
reg store_axis_int_to_temp;
reg store_axis_temp_to_output;

assign m_axis_tdata  = m_axis_tdata_reg;
assign m_axis_tkeep  = M_KEEP_ENABLE ? m_axis_tkeep_reg : {M_KEEP_WIDTH{1'b1}};
assign m_axis_tvalid = m_axis_tvalid_reg;
assign m_axis_tlast  = m_axis_tlast_reg;
assign m_axis_tid    = ID_ENABLE   ? m_axis_tid_reg   : {ID_WIDTH{1'b0}};
assign m_axis_tdest  = DEST_ENABLE ? m_axis_tdest_reg : {DEST_WIDTH{1'b0}};
assign m_axis_tuser  = USER_ENABLE ? m_axis_tuser_reg : {USER_WIDTH{1'b0}};

// enable ready input next cycle if output is ready or the temp reg will not be filled on the next cycle (output reg empty or no input)
assign m_axis_tready_int_early = m_axis_tready || (!temp_m_axis_tvalid_reg && (!m_axis_tvalid_reg || !m_axis_tvalid_int));

always @* begin
    // transfer sink ready state to source
    m_axis_tvalid_next = m_axis_tvalid_reg;
    temp_m_axis_tvalid_next = temp_m_axis_tvalid_reg;

    store_axis_int_to_output = 1'b0;
    store_axis_int_to_temp = 1'b0;
    store_axis_temp_to_output = 1'b0;

    if (m_axis_tready_int_reg) begin
        // input is ready
        if (m_axis_tready || !m_axis_tvalid_reg) begin
            // output is ready or currently not valid, transfer data to output
            m_axis_tvalid_next = m_axis_tvalid_int;
            store_axis_int_to_output = 1'b1;
        end else begin
            // output is not ready, store input in temp
            temp_m_axis_tvalid_next = m_axis_tvalid_int;
            store_axis_int_to_temp = 1'b1;
        end
    end else if (m_axis_tready) begin
        // input is not ready, but output is ready
        m_axis_tvalid_next = temp_m_axis_tvalid_reg;
        temp_m_axis_tvalid_next = 1'b0;
        store_axis_temp_to_output = 1'b1;
    end
end

always @(posedge clk) begin
    if (rst) begin
        m_axis_tvalid_reg <= 1'b0;
        m_axis_tready_int_reg <= 1'b0;
        temp_m_axis_tvalid_reg <= 1'b0;
    end else begin
        m_axis_tvalid_reg <= m_axis_tvalid_next;
        m_axis_tready_int_reg <= m_axis_tready_int_early;
        temp_m_axis_tvalid_reg <= temp_m_axis_tvalid_next;
    end

    // datapath
    if (store_axis_int_to_output) begin
        m_axis_tdata_reg <= m_axis_tdata_int;
        m_axis_tkeep_reg <= m_axis_tkeep_int;
        m_axis_tlast_reg <= m_axis_tlast_int;
        m_axis_tid_reg   <= m_axis_tid_int;
        m_axis_tdest_reg <= m_axis_tdest_int;
        m_axis_tuser_reg <= m_axis_tuser_int;
    end else if (store_axis_temp_to_output) begin
        m_axis_tdata_reg <= temp_m_axis_tdata_reg;
        m_axis_tkeep_reg <= temp_m_axis_tkeep_reg;
        m_axis_tlast_reg <= temp_m_axis_tlast_reg;
        m_axis_tid_reg   <= temp_m_axis_tid_reg;
        m_axis_tdest_reg <= temp_m_axis_tdest_reg;
        m_axis_tuser_reg <= temp_m_axis_tuser_reg;
    end

    if (store_axis_int_to_temp) begin
        temp_m_axis_tdata_reg <= m_axis_tdata_int;
        temp_m_axis_tkeep_reg <= m_axis_tkeep_int;
        temp_m_axis_tlast_reg <= m_axis_tlast_int;
        temp_m_axis_tid_reg   <= m_axis_tid_int;
        temp_m_axis_tdest_reg <= m_axis_tdest_int;
        temp_m_axis_tuser_reg <= m_axis_tuser_int;
    end
end

endmodule
/*

Copyright (c) 2018 Alex Forencich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

// Language: Verilog 2001

`timescale 1ns / 1ps

/*
 * AXI4 DMA
 */
module axi_dma #
(
    // Width of AXI data bus in bits
    parameter AXI_DATA_WIDTH = 32,
    // Width of AXI address bus in bits
    parameter AXI_ADDR_WIDTH = 16,
    // Width of AXI wstrb (width of data bus in words)
    parameter AXI_STRB_WIDTH = (AXI_DATA_WIDTH/8),
    // Width of AXI ID signal
    parameter AXI_ID_WIDTH = 8,
    // Maximum AXI burst length to generate
    parameter AXI_MAX_BURST_LEN = 16,
    // Width of AXI stream interfaces in bits
    parameter AXIS_DATA_WIDTH = AXI_DATA_WIDTH,
    // Use AXI stream tkeep signal
    parameter AXIS_KEEP_ENABLE = (AXIS_DATA_WIDTH>8),
    // AXI stream tkeep signal width (words per cycle)
    parameter AXIS_KEEP_WIDTH = (AXIS_DATA_WIDTH/8),
    // Use AXI stream tlast signal
    parameter AXIS_LAST_ENABLE = 1,
    // Propagate AXI stream tid signal
    parameter AXIS_ID_ENABLE = 0,
    // AXI stream tid signal width
    parameter AXIS_ID_WIDTH = 8,
    // Propagate AXI stream tdest signal
    parameter AXIS_DEST_ENABLE = 0,
    // AXI stream tdest signal width
    parameter AXIS_DEST_WIDTH = 8,
    // Propagate AXI stream tuser signal
    parameter AXIS_USER_ENABLE = 1,
    // AXI stream tuser signal width
    parameter AXIS_USER_WIDTH = 1,
    // Width of length field
    parameter LEN_WIDTH = 20,
    // Width of tag field
    parameter TAG_WIDTH = 8,
    // Enable support for scatter/gather DMA
    // (multiple descriptors per AXI stream frame)
    parameter ENABLE_SG = 0,
    // Enable support for unaligned transfers
    parameter ENABLE_UNALIGNED = 0
)
(
    input  wire                       clk,
    input  wire                       rstn,

    /*
     * AXI read descriptor input
     */
    input  wire [AXI_ADDR_WIDTH-1:0]  s_axis_read_desc_addr,
    input  wire [LEN_WIDTH-1:0]       s_axis_read_desc_len,
    input  wire [TAG_WIDTH-1:0]       s_axis_read_desc_tag,
    input  wire [AXIS_ID_WIDTH-1:0]   s_axis_read_desc_id,
    input  wire [AXIS_DEST_WIDTH-1:0] s_axis_read_desc_dest,
    input  wire [AXIS_USER_WIDTH-1:0] s_axis_read_desc_user,
    input  wire                       s_axis_read_desc_valid,
    output wire                       s_axis_read_desc_ready,

    /*
     * AXI read descriptor status output
     */
    output wire [TAG_WIDTH-1:0]       m_axis_read_desc_status_tag,
    output wire                       m_axis_read_desc_status_valid,

    /*
     * AXI stream read data output
     */
    output wire [AXIS_DATA_WIDTH-1:0] m_axis_read_data_tdata,
    output wire [AXIS_KEEP_WIDTH-1:0] m_axis_read_data_tkeep,
    output wire                       m_axis_read_data_tvalid,
    input  wire                       m_axis_read_data_tready,
    output wire                       m_axis_read_data_tlast,
    output wire [AXIS_ID_WIDTH-1:0]   m_axis_read_data_tid,
    output wire [AXIS_DEST_WIDTH-1:0] m_axis_read_data_tdest,
    output wire [AXIS_USER_WIDTH-1:0] m_axis_read_data_tuser,

    /*
     * AXI write descriptor input
     */
    input  wire [AXI_ADDR_WIDTH-1:0]  s_axis_write_desc_addr,
    input  wire [LEN_WIDTH-1:0]       s_axis_write_desc_len,
    input  wire [TAG_WIDTH-1:0]       s_axis_write_desc_tag,
    input  wire                       s_axis_write_desc_valid,
    output wire                       s_axis_write_desc_ready,

    /*
     * AXI write descriptor status output
     */
    output wire [LEN_WIDTH-1:0]       m_axis_write_desc_status_len,
    output wire [TAG_WIDTH-1:0]       m_axis_write_desc_status_tag,
    output wire [AXIS_ID_WIDTH-1:0]   m_axis_write_desc_status_id,
    output wire [AXIS_DEST_WIDTH-1:0] m_axis_write_desc_status_dest,
    output wire [AXIS_USER_WIDTH-1:0] m_axis_write_desc_status_user,
    output wire                       m_axis_write_desc_status_valid,

    /*
     * AXI stream write data input
     */
    input  wire [AXIS_DATA_WIDTH-1:0] s_axis_write_data_tdata,
    input  wire [AXIS_KEEP_WIDTH-1:0] s_axis_write_data_tkeep,
    input  wire                       s_axis_write_data_tvalid,
    output wire                       s_axis_write_data_tready,
    input  wire                       s_axis_write_data_tlast,
    input  wire [AXIS_ID_WIDTH-1:0]   s_axis_write_data_tid,
    input  wire [AXIS_DEST_WIDTH-1:0] s_axis_write_data_tdest,
    input  wire [AXIS_USER_WIDTH-1:0] s_axis_write_data_tuser,

    /*
     * AXI master interface
     */
    output wire [AXI_ID_WIDTH-1:0]    m_axi_awid,
    output wire [AXI_ADDR_WIDTH-1:0]  m_axi_awaddr,
    output wire [7:0]                 m_axi_awlen,
    output wire [2:0]                 m_axi_awsize,
    output wire [1:0]                 m_axi_awburst,
    output wire                       m_axi_awlock,
    output wire [3:0]                 m_axi_awcache,
    output wire [2:0]                 m_axi_awprot,
    output wire                       m_axi_awvalid,
    input  wire                       m_axi_awready,
    output wire [AXI_DATA_WIDTH-1:0]  m_axi_wdata,
    output wire [AXI_STRB_WIDTH-1:0]  m_axi_wstrb,
    output wire                       m_axi_wlast,
    output wire                       m_axi_wvalid,
    input  wire                       m_axi_wready,
    input  wire [AXI_ID_WIDTH-1:0]    m_axi_bid,
    input  wire [1:0]                 m_axi_bresp,
    input  wire                       m_axi_bvalid,
    output wire                       m_axi_bready,
    output wire [AXI_ID_WIDTH-1:0]    m_axi_arid,
    output wire [AXI_ADDR_WIDTH-1:0]  m_axi_araddr,
    output wire [7:0]                 m_axi_arlen,
    output wire [2:0]                 m_axi_arsize,
    output wire [1:0]                 m_axi_arburst,
    output wire                       m_axi_arlock,
    output wire [3:0]                 m_axi_arcache,
    output wire [2:0]                 m_axi_arprot,
    output wire                       m_axi_arvalid,
    input  wire                       m_axi_arready,
    input  wire [AXI_ID_WIDTH-1:0]    m_axi_rid,
    input  wire [AXI_DATA_WIDTH-1:0]  m_axi_rdata,
    input  wire [1:0]                 m_axi_rresp,
    input  wire                       m_axi_rlast,
    input  wire                       m_axi_rvalid,
    output wire                       m_axi_rready,

    /*
     * Configuration
     */
    input  wire                       read_enable,
    input  wire                       write_enable,
    input  wire                       write_abort
);

wire rst = ~rstn;

axi_dma_rd #(
    .AXI_DATA_WIDTH(AXI_DATA_WIDTH),
    .AXI_ADDR_WIDTH(AXI_ADDR_WIDTH),
    .AXI_STRB_WIDTH(AXI_STRB_WIDTH),
    .AXI_ID_WIDTH(AXI_ID_WIDTH),
    .AXI_MAX_BURST_LEN(AXI_MAX_BURST_LEN),
    .AXIS_DATA_WIDTH(AXIS_DATA_WIDTH),
    .AXIS_KEEP_ENABLE(AXIS_KEEP_ENABLE),
    .AXIS_KEEP_WIDTH(AXIS_KEEP_WIDTH),
    .AXIS_LAST_ENABLE(AXIS_LAST_ENABLE),
    .AXIS_ID_ENABLE(AXIS_ID_ENABLE),
    .AXIS_ID_WIDTH(AXIS_ID_WIDTH),
    .AXIS_DEST_ENABLE(AXIS_DEST_ENABLE),
    .AXIS_DEST_WIDTH(AXIS_DEST_WIDTH),
    .AXIS_USER_ENABLE(AXIS_USER_ENABLE),
    .AXIS_USER_WIDTH(AXIS_USER_WIDTH),
    .LEN_WIDTH(LEN_WIDTH),
    .TAG_WIDTH(TAG_WIDTH),
    .ENABLE_SG(ENABLE_SG),
    .ENABLE_UNALIGNED(ENABLE_UNALIGNED)
)
axi_dma_rd_inst (
    .clk(clk),
    .rst(rst),

    /*
     * AXI read descriptor input
     */
    .s_axis_read_desc_addr(s_axis_read_desc_addr),
    .s_axis_read_desc_len(s_axis_read_desc_len),
    .s_axis_read_desc_tag(s_axis_read_desc_tag),
    .s_axis_read_desc_id(s_axis_read_desc_id),
    .s_axis_read_desc_dest(s_axis_read_desc_dest),
    .s_axis_read_desc_user(s_axis_read_desc_user),
    .s_axis_read_desc_valid(s_axis_read_desc_valid),
    .s_axis_read_desc_ready(s_axis_read_desc_ready),

    /*
     * AXI read descriptor status output
     */
    .m_axis_read_desc_status_tag(m_axis_read_desc_status_tag),
    .m_axis_read_desc_status_valid(m_axis_read_desc_status_valid),

    /*
     * AXI stream read data output
     */
    .m_axis_read_data_tdata(m_axis_read_data_tdata),
    .m_axis_read_data_tkeep(m_axis_read_data_tkeep),
    .m_axis_read_data_tvalid(m_axis_read_data_tvalid),
    .m_axis_read_data_tready(m_axis_read_data_tready),
    .m_axis_read_data_tlast(m_axis_read_data_tlast),
    .m_axis_read_data_tid(m_axis_read_data_tid),
    .m_axis_read_data_tdest(m_axis_read_data_tdest),
    .m_axis_read_data_tuser(m_axis_read_data_tuser),

    /*
     * AXI master interface
     */
    .m_axi_arid(m_axi_arid),
    .m_axi_araddr(m_axi_araddr),
    .m_axi_arlen(m_axi_arlen),
    .m_axi_arsize(m_axi_arsize),
    .m_axi_arburst(m_axi_arburst),
    .m_axi_arlock(m_axi_arlock),
    .m_axi_arcache(m_axi_arcache),
    .m_axi_arprot(m_axi_arprot),
    .m_axi_arvalid(m_axi_arvalid),
    .m_axi_arready(m_axi_arready),
    .m_axi_rid(m_axi_rid),
    .m_axi_rdata(m_axi_rdata),
    .m_axi_rresp(m_axi_rresp),
    .m_axi_rlast(m_axi_rlast),
    .m_axi_rvalid(m_axi_rvalid),
    .m_axi_rready(m_axi_rready),

    /*
     * Configuration
     */
    .enable(read_enable)
);

axi_dma_wr #(
    .AXI_DATA_WIDTH(AXI_DATA_WIDTH),
    .AXI_ADDR_WIDTH(AXI_ADDR_WIDTH),
    .AXI_STRB_WIDTH(AXI_STRB_WIDTH),
    .AXI_ID_WIDTH(AXI_ID_WIDTH),
    .AXI_MAX_BURST_LEN(AXI_MAX_BURST_LEN),
    .AXIS_DATA_WIDTH(AXIS_DATA_WIDTH),
    .AXIS_KEEP_ENABLE(AXIS_KEEP_ENABLE),
    .AXIS_KEEP_WIDTH(AXIS_KEEP_WIDTH),
    .AXIS_LAST_ENABLE(AXIS_LAST_ENABLE),
    .AXIS_ID_ENABLE(AXIS_ID_ENABLE),
    .AXIS_ID_WIDTH(AXIS_ID_WIDTH),
    .AXIS_DEST_ENABLE(AXIS_DEST_ENABLE),
    .AXIS_DEST_WIDTH(AXIS_DEST_WIDTH),
    .AXIS_USER_ENABLE(AXIS_USER_ENABLE),
    .AXIS_USER_WIDTH(AXIS_USER_WIDTH),
    .LEN_WIDTH(LEN_WIDTH),
    .TAG_WIDTH(TAG_WIDTH),
    .ENABLE_SG(ENABLE_SG),
    .ENABLE_UNALIGNED(ENABLE_UNALIGNED)
)
axi_dma_wr_inst (
    .clk(clk),
    .rst(rst),

    /*
     * AXI write descriptor input
     */
    .s_axis_write_desc_addr(s_axis_write_desc_addr),
    .s_axis_write_desc_len(s_axis_write_desc_len),
    .s_axis_write_desc_tag(s_axis_write_desc_tag),
    .s_axis_write_desc_valid(s_axis_write_desc_valid),
    .s_axis_write_desc_ready(s_axis_write_desc_ready),

    /*
     * AXI write descriptor status output
     */
    .m_axis_write_desc_status_len(m_axis_write_desc_status_len),
    .m_axis_write_desc_status_tag(m_axis_write_desc_status_tag),
    .m_axis_write_desc_status_id(m_axis_write_desc_status_id),
    .m_axis_write_desc_status_dest(m_axis_write_desc_status_dest),
    .m_axis_write_desc_status_user(m_axis_write_desc_status_user),
    .m_axis_write_desc_status_valid(m_axis_write_desc_status_valid),

    /*
     * AXI stream write data input
     */
    .s_axis_write_data_tdata(s_axis_write_data_tdata),
    .s_axis_write_data_tkeep(s_axis_write_data_tkeep),
    .s_axis_write_data_tvalid(s_axis_write_data_tvalid),
    .s_axis_write_data_tready(s_axis_write_data_tready),
    .s_axis_write_data_tlast(s_axis_write_data_tlast),
    .s_axis_write_data_tid(s_axis_write_data_tid),
    .s_axis_write_data_tdest(s_axis_write_data_tdest),
    .s_axis_write_data_tuser(s_axis_write_data_tuser),

    /*
     * AXI master interface
     */
    .m_axi_awid(m_axi_awid),
    .m_axi_awaddr(m_axi_awaddr),
    .m_axi_awlen(m_axi_awlen),
    .m_axi_awsize(m_axi_awsize),
    .m_axi_awburst(m_axi_awburst),
    .m_axi_awlock(m_axi_awlock),
    .m_axi_awcache(m_axi_awcache),
    .m_axi_awprot(m_axi_awprot),
    .m_axi_awvalid(m_axi_awvalid),
    .m_axi_awready(m_axi_awready),
    .m_axi_wdata(m_axi_wdata),
    .m_axi_wstrb(m_axi_wstrb),
    .m_axi_wlast(m_axi_wlast),
    .m_axi_wvalid(m_axi_wvalid),
    .m_axi_wready(m_axi_wready),
    .m_axi_bid(m_axi_bid),
    .m_axi_bresp(m_axi_bresp),
    .m_axi_bvalid(m_axi_bvalid),
    .m_axi_bready(m_axi_bready),

    /*
     * Configuration
     */
    .enable(write_enable),
    .abort(write_abort)
);

endmodule
/*

Copyright (c) 2018 Alex Forencich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

// Language: Verilog 2001

`timescale 1ns / 1ps

/*
 * AXI4 DMA
 */
module axi_dma_rd #
(
    // Width of AXI data bus in bits
    parameter AXI_DATA_WIDTH = 32,
    // Width of AXI address bus in bits
    parameter AXI_ADDR_WIDTH = 16,
    // Width of AXI wstrb (width of data bus in words)
    parameter AXI_STRB_WIDTH = (AXI_DATA_WIDTH/8),
    // Width of AXI ID signal
    parameter AXI_ID_WIDTH = 8,
    // Maximum AXI burst length to generate
    parameter AXI_MAX_BURST_LEN = 16,
    // Width of AXI stream interfaces in bits
    parameter AXIS_DATA_WIDTH = AXI_DATA_WIDTH,
    // Use AXI stream tkeep signal
    parameter AXIS_KEEP_ENABLE = (AXIS_DATA_WIDTH>8),
    // AXI stream tkeep signal width (words per cycle)
    parameter AXIS_KEEP_WIDTH = (AXIS_DATA_WIDTH/8),
    // Use AXI stream tlast signal
    parameter AXIS_LAST_ENABLE = 1,
    // Propagate AXI stream tid signal
    parameter AXIS_ID_ENABLE = 0,
    // AXI stream tid signal width
    parameter AXIS_ID_WIDTH = 8,
    // Propagate AXI stream tdest signal
    parameter AXIS_DEST_ENABLE = 0,
    // AXI stream tdest signal width
    parameter AXIS_DEST_WIDTH = 8,
    // Propagate AXI stream tuser signal
    parameter AXIS_USER_ENABLE = 1,
    // AXI stream tuser signal width
    parameter AXIS_USER_WIDTH = 1,
    // Width of length field
    parameter LEN_WIDTH = 20,
    // Width of tag field
    parameter TAG_WIDTH = 8,
    // Enable support for scatter/gather DMA
    // (multiple descriptors per AXI stream frame)
    parameter ENABLE_SG = 0,
    // Enable support for unaligned transfers
    parameter ENABLE_UNALIGNED = 0
)
(
    input  wire                       clk,
    input  wire                       rst,

    /*
     * AXI read descriptor input
     */
    input  wire [AXI_ADDR_WIDTH-1:0]  s_axis_read_desc_addr,
    input  wire [LEN_WIDTH-1:0]       s_axis_read_desc_len,
    input  wire [TAG_WIDTH-1:0]       s_axis_read_desc_tag,
    input  wire [AXIS_ID_WIDTH-1:0]   s_axis_read_desc_id,
    input  wire [AXIS_DEST_WIDTH-1:0] s_axis_read_desc_dest,
    input  wire [AXIS_USER_WIDTH-1:0] s_axis_read_desc_user,
    input  wire                       s_axis_read_desc_valid,
    output wire                       s_axis_read_desc_ready,

    /*
     * AXI read descriptor status output
     */
    output wire [TAG_WIDTH-1:0]       m_axis_read_desc_status_tag,
    output wire                       m_axis_read_desc_status_valid,

    /*
     * AXI stream read data output
     */
    output wire [AXIS_DATA_WIDTH-1:0] m_axis_read_data_tdata,
    output wire [AXIS_KEEP_WIDTH-1:0] m_axis_read_data_tkeep,
    output wire                       m_axis_read_data_tvalid,
    input  wire                       m_axis_read_data_tready,
    output wire                       m_axis_read_data_tlast,
    output wire [AXIS_ID_WIDTH-1:0]   m_axis_read_data_tid,
    output wire [AXIS_DEST_WIDTH-1:0] m_axis_read_data_tdest,
    output wire [AXIS_USER_WIDTH-1:0] m_axis_read_data_tuser,

    /*
     * AXI master interface
     */
    output wire [AXI_ID_WIDTH-1:0]    m_axi_arid,
    output wire [AXI_ADDR_WIDTH-1:0]  m_axi_araddr,
    output wire [7:0]                 m_axi_arlen,
    output wire [2:0]                 m_axi_arsize,
    output wire [1:0]                 m_axi_arburst,
    output wire                       m_axi_arlock,
    output wire [3:0]                 m_axi_arcache,
    output wire [2:0]                 m_axi_arprot,
    output wire                       m_axi_arvalid,
    input  wire                       m_axi_arready,
    input  wire [AXI_ID_WIDTH-1:0]    m_axi_rid,
    input  wire [AXI_DATA_WIDTH-1:0]  m_axi_rdata,
    input  wire [1:0]                 m_axi_rresp,
    input  wire                       m_axi_rlast,
    input  wire                       m_axi_rvalid,
    output wire                       m_axi_rready,

    /*
     * Configuration
     */
    input  wire                       enable
);

parameter AXI_WORD_WIDTH = AXI_STRB_WIDTH;
parameter AXI_WORD_SIZE = AXI_DATA_WIDTH/AXI_WORD_WIDTH;
parameter AXI_BURST_SIZE = $clog2(AXI_STRB_WIDTH);
parameter AXI_MAX_BURST_SIZE = AXI_MAX_BURST_LEN << AXI_BURST_SIZE;

parameter AXIS_KEEP_WIDTH_INT = AXIS_KEEP_ENABLE ? AXIS_KEEP_WIDTH : 1;
parameter AXIS_WORD_WIDTH = AXIS_KEEP_WIDTH_INT;
parameter AXIS_WORD_SIZE = AXIS_DATA_WIDTH/AXIS_WORD_WIDTH;

parameter OFFSET_WIDTH = AXI_STRB_WIDTH > 1 ? $clog2(AXI_STRB_WIDTH) : 1;
parameter OFFSET_MASK = AXI_STRB_WIDTH > 1 ? {OFFSET_WIDTH{1'b1}} : 0;
parameter ADDR_MASK = {AXI_ADDR_WIDTH{1'b1}} << $clog2(AXI_STRB_WIDTH);
parameter CYCLE_COUNT_WIDTH = LEN_WIDTH - AXI_BURST_SIZE + 1;

// // bus width assertions
// initial begin
//     if (AXI_WORD_SIZE * AXI_STRB_WIDTH != AXI_DATA_WIDTH) begin
//         $error("Error: AXI data width not evenly divisble (instance %m)");
//         $finish;
//     end
// 
//     if (AXIS_WORD_SIZE * AXIS_KEEP_WIDTH_INT != AXIS_DATA_WIDTH) begin
//         $error("Error: AXI stream data width not evenly divisble (instance %m)");
//         $finish;
//     end
// 
//     if (AXI_WORD_SIZE != AXIS_WORD_SIZE) begin
//         $error("Error: word size mismatch (instance %m)");
//         $finish;
//     end
// 
//     if (2**$clog2(AXI_WORD_WIDTH) != AXI_WORD_WIDTH) begin
//         $error("Error: AXI word width must be even power of two (instance %m)");
//         $finish;
//     end
// 
//     if (AXI_DATA_WIDTH != AXIS_DATA_WIDTH) begin
//         $error("Error: AXI interface width must match AXI stream interface width (instance %m)");
//         $finish;
//     end
// 
//     if (AXI_MAX_BURST_LEN < 1 || AXI_MAX_BURST_LEN > 256) begin
//         $error("Error: AXI_MAX_BURST_LEN must be between 1 and 256 (instance %m)");
//         $finish;
//     end
// 
//     if (ENABLE_SG) begin
//         $error("Error: scatter/gather is not yet implemented (instance %m)");
//         $finish;
//     end
// end

localparam [0:0]
    AXI_STATE_IDLE = 1'd0,
    AXI_STATE_START = 1'd1;

reg [0:0] axi_state_reg = AXI_STATE_IDLE, axi_state_next;

localparam [0:0]
    AXIS_STATE_IDLE = 1'd0,
    AXIS_STATE_READ = 1'd1;

reg [0:0] axis_state_reg = AXIS_STATE_IDLE, axis_state_next;

// datapath control signals
reg transfer_in_save;
reg axis_cmd_ready;

reg [AXI_ADDR_WIDTH-1:0] addr_reg = {AXI_ADDR_WIDTH{1'b0}}, addr_next;
reg [LEN_WIDTH-1:0] op_word_count_reg = {LEN_WIDTH{1'b0}}, op_word_count_next;
reg [LEN_WIDTH-1:0] tr_word_count_reg = {LEN_WIDTH{1'b0}}, tr_word_count_next;

reg [OFFSET_WIDTH-1:0] axis_cmd_offset_reg = {OFFSET_WIDTH{1'b0}}, axis_cmd_offset_next;
reg [OFFSET_WIDTH-1:0] axis_cmd_last_cycle_offset_reg = {OFFSET_WIDTH{1'b0}}, axis_cmd_last_cycle_offset_next;
reg [CYCLE_COUNT_WIDTH-1:0] axis_cmd_input_cycle_count_reg = {CYCLE_COUNT_WIDTH{1'b0}}, axis_cmd_input_cycle_count_next;
reg [CYCLE_COUNT_WIDTH-1:0] axis_cmd_output_cycle_count_reg = {CYCLE_COUNT_WIDTH{1'b0}}, axis_cmd_output_cycle_count_next;
reg axis_cmd_bubble_cycle_reg = 1'b0, axis_cmd_bubble_cycle_next;
reg [TAG_WIDTH-1:0] axis_cmd_tag_reg = {TAG_WIDTH{1'b0}}, axis_cmd_tag_next;
reg [AXIS_ID_WIDTH-1:0] axis_cmd_axis_id_reg = {AXIS_ID_WIDTH{1'b0}}, axis_cmd_axis_id_next;
reg [AXIS_DEST_WIDTH-1:0] axis_cmd_axis_dest_reg = {AXIS_DEST_WIDTH{1'b0}}, axis_cmd_axis_dest_next;
reg [AXIS_USER_WIDTH-1:0] axis_cmd_axis_user_reg = {AXIS_USER_WIDTH{1'b0}}, axis_cmd_axis_user_next;
reg axis_cmd_valid_reg = 1'b0, axis_cmd_valid_next;

reg [OFFSET_WIDTH-1:0] offset_reg = {OFFSET_WIDTH{1'b0}}, offset_next;
reg [OFFSET_WIDTH-1:0] last_cycle_offset_reg = {OFFSET_WIDTH{1'b0}}, last_cycle_offset_next;
reg [CYCLE_COUNT_WIDTH-1:0] input_cycle_count_reg = {CYCLE_COUNT_WIDTH{1'b0}}, input_cycle_count_next;
reg [CYCLE_COUNT_WIDTH-1:0] output_cycle_count_reg = {CYCLE_COUNT_WIDTH{1'b0}}, output_cycle_count_next;
reg input_active_reg = 1'b0, input_active_next;
reg output_active_reg = 1'b0, output_active_next;
reg bubble_cycle_reg = 1'b0, bubble_cycle_next;
reg first_cycle_reg = 1'b0, first_cycle_next;
reg output_last_cycle_reg = 1'b0, output_last_cycle_next;

reg [TAG_WIDTH-1:0] tag_reg = {TAG_WIDTH{1'b0}}, tag_next;
reg [AXIS_ID_WIDTH-1:0] axis_id_reg = {AXIS_ID_WIDTH{1'b0}}, axis_id_next;
reg [AXIS_DEST_WIDTH-1:0] axis_dest_reg = {AXIS_DEST_WIDTH{1'b0}}, axis_dest_next;
reg [AXIS_USER_WIDTH-1:0] axis_user_reg = {AXIS_USER_WIDTH{1'b0}}, axis_user_next;

reg s_axis_read_desc_ready_reg = 1'b0, s_axis_read_desc_ready_next;

reg [TAG_WIDTH-1:0] m_axis_read_desc_status_tag_reg = {TAG_WIDTH{1'b0}}, m_axis_read_desc_status_tag_next;
reg m_axis_read_desc_status_valid_reg = 1'b0, m_axis_read_desc_status_valid_next;

reg [AXI_ADDR_WIDTH-1:0] m_axi_araddr_reg = {AXI_ADDR_WIDTH{1'b0}}, m_axi_araddr_next;
reg [7:0] m_axi_arlen_reg = 8'd0, m_axi_arlen_next;
reg m_axi_arvalid_reg = 1'b0, m_axi_arvalid_next;
reg m_axi_rready_reg = 1'b0, m_axi_rready_next;

reg [AXI_DATA_WIDTH-1:0] save_axi_rdata_reg = {AXI_DATA_WIDTH{1'b0}};

wire [AXI_DATA_WIDTH-1:0] shift_axi_rdata = {m_axi_rdata, save_axi_rdata_reg} >> ((AXI_STRB_WIDTH-offset_reg)*AXI_WORD_SIZE);

// internal datapath
reg  [AXIS_DATA_WIDTH-1:0] m_axis_read_data_tdata_int;
reg  [AXIS_KEEP_WIDTH-1:0] m_axis_read_data_tkeep_int;
reg                        m_axis_read_data_tvalid_int;
reg                        m_axis_read_data_tready_int_reg = 1'b0;
reg                        m_axis_read_data_tlast_int;
reg  [AXIS_ID_WIDTH-1:0]   m_axis_read_data_tid_int;
reg  [AXIS_DEST_WIDTH-1:0] m_axis_read_data_tdest_int;
reg  [AXIS_USER_WIDTH-1:0] m_axis_read_data_tuser_int;
wire                       m_axis_read_data_tready_int_early;

assign s_axis_read_desc_ready = s_axis_read_desc_ready_reg;

assign m_axis_read_desc_status_tag = m_axis_read_desc_status_tag_reg;
assign m_axis_read_desc_status_valid = m_axis_read_desc_status_valid_reg;

assign m_axi_arid = {AXI_ID_WIDTH{1'b0}};
assign m_axi_araddr = m_axi_araddr_reg;
assign m_axi_arlen = m_axi_arlen_reg;
assign m_axi_arsize = AXI_BURST_SIZE;
assign m_axi_arburst = 2'b01;
assign m_axi_arlock = 1'b0;
assign m_axi_arcache = 4'b0011;
assign m_axi_arprot = 3'b010;
assign m_axi_arvalid = m_axi_arvalid_reg;
assign m_axi_rready = m_axi_rready_reg;

wire [AXI_ADDR_WIDTH-1:0] addr_plus_max_burst = addr_reg + AXI_MAX_BURST_SIZE;
wire [AXI_ADDR_WIDTH-1:0] addr_plus_count = addr_reg + op_word_count_reg;

always @* begin
    axi_state_next = AXI_STATE_IDLE;

    s_axis_read_desc_ready_next = 1'b0;

    m_axi_araddr_next = m_axi_araddr_reg;
    m_axi_arlen_next = m_axi_arlen_reg;
    m_axi_arvalid_next = m_axi_arvalid_reg && !m_axi_arready;

    addr_next = addr_reg;
    op_word_count_next = op_word_count_reg;
    tr_word_count_next = tr_word_count_reg;

    axis_cmd_offset_next = axis_cmd_offset_reg;
    axis_cmd_last_cycle_offset_next = axis_cmd_last_cycle_offset_reg;
    axis_cmd_input_cycle_count_next = axis_cmd_input_cycle_count_reg;
    axis_cmd_output_cycle_count_next = axis_cmd_output_cycle_count_reg;
    axis_cmd_bubble_cycle_next = axis_cmd_bubble_cycle_reg;
    axis_cmd_tag_next = axis_cmd_tag_reg;
    axis_cmd_axis_id_next = axis_cmd_axis_id_reg;
    axis_cmd_axis_dest_next = axis_cmd_axis_dest_reg;
    axis_cmd_axis_user_next = axis_cmd_axis_user_reg;
    axis_cmd_valid_next = axis_cmd_valid_reg && !axis_cmd_ready;

    case (axi_state_reg)
        AXI_STATE_IDLE: begin
            // idle state - load new descriptor to start operation
            s_axis_read_desc_ready_next = !axis_cmd_valid_reg && enable;

            if (s_axis_read_desc_ready && s_axis_read_desc_valid) begin
                if (ENABLE_UNALIGNED) begin
                    addr_next = s_axis_read_desc_addr;
                    axis_cmd_offset_next = AXI_STRB_WIDTH > 1 ? AXI_STRB_WIDTH - (s_axis_read_desc_addr & OFFSET_MASK) : 0;
                    axis_cmd_bubble_cycle_next = axis_cmd_offset_next > 0;
                    axis_cmd_last_cycle_offset_next = s_axis_read_desc_len & OFFSET_MASK;
                end else begin
                    addr_next = s_axis_read_desc_addr & ADDR_MASK;
                    axis_cmd_offset_next = 0;
                    axis_cmd_bubble_cycle_next = 1'b0;
                    axis_cmd_last_cycle_offset_next = s_axis_read_desc_len & OFFSET_MASK;
                end
                axis_cmd_tag_next = s_axis_read_desc_tag;
                op_word_count_next = s_axis_read_desc_len;

                axis_cmd_axis_id_next = s_axis_read_desc_id;
                axis_cmd_axis_dest_next = s_axis_read_desc_dest;
                axis_cmd_axis_user_next = s_axis_read_desc_user;

                if (ENABLE_UNALIGNED) begin
                    axis_cmd_input_cycle_count_next = (op_word_count_next + (s_axis_read_desc_addr & OFFSET_MASK) - 1) >> AXI_BURST_SIZE;
                end else begin
                    axis_cmd_input_cycle_count_next = (op_word_count_next - 1) >> AXI_BURST_SIZE;
                end
                axis_cmd_output_cycle_count_next = (op_word_count_next - 1) >> AXI_BURST_SIZE;

                axis_cmd_valid_next = 1'b1;

                s_axis_read_desc_ready_next = 1'b0;
                axi_state_next = AXI_STATE_START;
            end else begin
                axi_state_next = AXI_STATE_IDLE;
            end
        end
        AXI_STATE_START: begin
            // start state - initiate new AXI transfer
            if (!m_axi_arvalid) begin
                if (op_word_count_reg <= AXI_MAX_BURST_SIZE - (addr_reg & OFFSET_MASK) || AXI_MAX_BURST_SIZE >= 4096) begin
                    // packet smaller than max burst size
                    if (addr_reg[12] != addr_plus_count[12]) begin
                        // crosses 4k boundary
                        tr_word_count_next = 13'h1000 - addr_reg[11:0];
                    end else begin
                        // does not cross 4k boundary
                        tr_word_count_next = op_word_count_reg;
                    end
                end else begin
                    // packet larger than max burst size
                    if (addr_reg[12] != addr_plus_max_burst[12]) begin
                        // crosses 4k boundary
                        tr_word_count_next = 13'h1000 - addr_reg[11:0];
                    end else begin
                        // does not cross 4k boundary
                        tr_word_count_next = AXI_MAX_BURST_SIZE - (addr_reg & OFFSET_MASK);
                    end
                end

                m_axi_araddr_next = addr_reg;
                if (ENABLE_UNALIGNED) begin
                    m_axi_arlen_next = (tr_word_count_next + (addr_reg & OFFSET_MASK) - 1) >> AXI_BURST_SIZE;
                end else begin
                    m_axi_arlen_next = (tr_word_count_next - 1) >> AXI_BURST_SIZE;
                end
                m_axi_arvalid_next = 1'b1;

                addr_next = addr_reg + tr_word_count_next;
                op_word_count_next = op_word_count_reg - tr_word_count_next;

                if (op_word_count_next > 0) begin
                    axi_state_next = AXI_STATE_START;
                end else begin
                    s_axis_read_desc_ready_next = !axis_cmd_valid_reg && enable;
                    axi_state_next = AXI_STATE_IDLE;
                end
            end else begin
                axi_state_next = AXI_STATE_START;
            end
        end
    endcase
end

always @* begin
    axis_state_next = AXIS_STATE_IDLE;

    m_axis_read_desc_status_tag_next = m_axis_read_desc_status_tag_reg;
    m_axis_read_desc_status_valid_next = 1'b0;

    m_axis_read_data_tdata_int = shift_axi_rdata;
    m_axis_read_data_tkeep_int = {AXIS_KEEP_WIDTH{1'b1}};
    m_axis_read_data_tlast_int = 1'b0;
    m_axis_read_data_tvalid_int = 1'b0;
    m_axis_read_data_tid_int = axis_id_reg;
    m_axis_read_data_tdest_int = axis_dest_reg;
    m_axis_read_data_tuser_int = axis_user_reg;

    m_axi_rready_next = 1'b0;

    transfer_in_save = 1'b0;
    axis_cmd_ready = 1'b0;

    offset_next = offset_reg;
    last_cycle_offset_next = last_cycle_offset_reg;
    input_cycle_count_next = input_cycle_count_reg;
    output_cycle_count_next = output_cycle_count_reg;
    input_active_next = input_active_reg;
    output_active_next = output_active_reg;
    bubble_cycle_next = bubble_cycle_reg;
    first_cycle_next = first_cycle_reg;
    output_last_cycle_next = output_last_cycle_reg;

    tag_next = tag_reg;
    axis_id_next = axis_id_reg;
    axis_dest_next = axis_dest_reg;
    axis_user_next = axis_user_reg;

    case (axis_state_reg)
        AXIS_STATE_IDLE: begin
            // idle state - load new descriptor to start operation
            m_axi_rready_next = 1'b0;

            // store transfer parameters
            if (ENABLE_UNALIGNED) begin
                offset_next = axis_cmd_offset_reg;
            end else begin
                offset_next = 0;
            end
            last_cycle_offset_next = axis_cmd_last_cycle_offset_reg;
            input_cycle_count_next = axis_cmd_input_cycle_count_reg;
            output_cycle_count_next = axis_cmd_output_cycle_count_reg;
            bubble_cycle_next = axis_cmd_bubble_cycle_reg;
            tag_next = axis_cmd_tag_reg;
            axis_id_next = axis_cmd_axis_id_reg;
            axis_dest_next = axis_cmd_axis_dest_reg;
            axis_user_next = axis_cmd_axis_user_reg;

            output_last_cycle_next = output_cycle_count_next == 0;
            input_active_next = 1'b1;
            output_active_next = 1'b1;
            first_cycle_next = 1'b1;

            if (axis_cmd_valid_reg) begin
                axis_cmd_ready = 1'b1;
                m_axi_rready_next = m_axis_read_data_tready_int_early;
                axis_state_next = AXIS_STATE_READ;
            end
        end
        AXIS_STATE_READ: begin
            // handle AXI read data
            m_axi_rready_next = m_axis_read_data_tready_int_early && input_active_reg;

            if (m_axis_read_data_tready_int_reg && ((m_axi_rready && m_axi_rvalid) || !input_active_reg)) begin
                // transfer in AXI read data
                transfer_in_save = m_axi_rready && m_axi_rvalid;

                if (ENABLE_UNALIGNED && first_cycle_reg && bubble_cycle_reg) begin
                    if (input_active_reg) begin
                        input_cycle_count_next = input_cycle_count_reg - 1;
                        input_active_next = input_cycle_count_reg > 0;
                    end
                    bubble_cycle_next = 1'b0;
                    first_cycle_next = 1'b0;

                    m_axi_rready_next = m_axis_read_data_tready_int_early && input_active_next;
                    axis_state_next = AXIS_STATE_READ;
                end else begin
                    // update counters
                    if (input_active_reg) begin
                        input_cycle_count_next = input_cycle_count_reg - 1;
                        input_active_next = input_cycle_count_reg > 0;
                    end
                    if (output_active_reg) begin
                        output_cycle_count_next = output_cycle_count_reg - 1;
                        output_active_next = output_cycle_count_reg > 0;
                    end
                    output_last_cycle_next = output_cycle_count_next == 0;
                    bubble_cycle_next = 1'b0;
                    first_cycle_next = 1'b0;

                    // pass through read data
                    m_axis_read_data_tdata_int = shift_axi_rdata;
                    m_axis_read_data_tkeep_int = {AXIS_KEEP_WIDTH_INT{1'b1}};
                    m_axis_read_data_tvalid_int = 1'b1;

                    if (output_last_cycle_reg) begin
                        // no more data to transfer, finish operation
                        if (last_cycle_offset_reg > 0) begin
                            m_axis_read_data_tkeep_int = {AXIS_KEEP_WIDTH_INT{1'b1}} >> (AXIS_KEEP_WIDTH_INT - last_cycle_offset_reg);
                        end
                        m_axis_read_data_tlast_int = 1'b1;

                        m_axis_read_desc_status_tag_next = tag_reg;
                        m_axis_read_desc_status_valid_next = 1'b1;

                        m_axi_rready_next = 1'b0;
                        axis_state_next = AXIS_STATE_IDLE;
                    end else begin
                        // more cycles in AXI transfer
                        m_axi_rready_next = m_axis_read_data_tready_int_early && input_active_next;
                        axis_state_next = AXIS_STATE_READ;
                    end
                end
            end else begin
                axis_state_next = AXIS_STATE_READ;
            end
        end
    endcase
end

always @(posedge clk) begin
    if (rst) begin
        axi_state_reg <= AXI_STATE_IDLE;
        axis_state_reg <= AXIS_STATE_IDLE;
        axis_cmd_valid_reg <= 1'b0;
        s_axis_read_desc_ready_reg <= 1'b0;
        m_axis_read_desc_status_valid_reg <= 1'b0;
        m_axi_arvalid_reg <= 1'b0;
        m_axi_rready_reg <= 1'b0;
    end else begin
        axi_state_reg <= axi_state_next;
        axis_state_reg <= axis_state_next;
        axis_cmd_valid_reg <= axis_cmd_valid_next;
        s_axis_read_desc_ready_reg <= s_axis_read_desc_ready_next;
        m_axis_read_desc_status_valid_reg <= m_axis_read_desc_status_valid_next;
        m_axi_arvalid_reg <= m_axi_arvalid_next;
        m_axi_rready_reg <= m_axi_rready_next;
    end

    m_axis_read_desc_status_tag_reg <= m_axis_read_desc_status_tag_next;

    m_axi_araddr_reg <= m_axi_araddr_next;
    m_axi_arlen_reg <= m_axi_arlen_next;

    addr_reg <= addr_next;
    op_word_count_reg <= op_word_count_next;
    tr_word_count_reg <= tr_word_count_next;

    axis_cmd_offset_reg <= axis_cmd_offset_next;
    axis_cmd_last_cycle_offset_reg <= axis_cmd_last_cycle_offset_next;
    axis_cmd_input_cycle_count_reg <= axis_cmd_input_cycle_count_next;
    axis_cmd_output_cycle_count_reg <= axis_cmd_output_cycle_count_next;
    axis_cmd_bubble_cycle_reg <= axis_cmd_bubble_cycle_next;
    axis_cmd_tag_reg <= axis_cmd_tag_next;
    axis_cmd_axis_id_reg <= axis_cmd_axis_id_next;
    axis_cmd_axis_dest_reg <= axis_cmd_axis_dest_next;
    axis_cmd_axis_user_reg <= axis_cmd_axis_user_next;
    axis_cmd_valid_reg <= axis_cmd_valid_next;

    offset_reg <= offset_next;
    last_cycle_offset_reg <= last_cycle_offset_next;
    input_cycle_count_reg <= input_cycle_count_next;
    output_cycle_count_reg <= output_cycle_count_next;
    input_active_reg <= input_active_next;
    output_active_reg <= output_active_next;
    bubble_cycle_reg <= bubble_cycle_next;
    first_cycle_reg <= first_cycle_next;
    output_last_cycle_reg <= output_last_cycle_next;

    tag_reg <= tag_next;
    axis_id_reg <= axis_id_next;
    axis_dest_reg <= axis_dest_next;
    axis_user_reg <= axis_user_next;

    if (transfer_in_save) begin
        save_axi_rdata_reg <= m_axi_rdata;
    end
end

// output datapath logic
reg [AXIS_DATA_WIDTH-1:0] m_axis_read_data_tdata_reg  = {AXIS_DATA_WIDTH{1'b0}};
reg [AXIS_KEEP_WIDTH-1:0] m_axis_read_data_tkeep_reg  = {AXIS_KEEP_WIDTH{1'b0}};
reg                       m_axis_read_data_tvalid_reg = 1'b0, m_axis_read_data_tvalid_next;
reg                       m_axis_read_data_tlast_reg  = 1'b0;
reg [AXIS_ID_WIDTH-1:0]   m_axis_read_data_tid_reg    = {AXIS_ID_WIDTH{1'b0}};
reg [AXIS_DEST_WIDTH-1:0] m_axis_read_data_tdest_reg  = {AXIS_DEST_WIDTH{1'b0}};
reg [AXIS_USER_WIDTH-1:0] m_axis_read_data_tuser_reg  = {AXIS_USER_WIDTH{1'b0}};

reg [AXIS_DATA_WIDTH-1:0] temp_m_axis_read_data_tdata_reg  = {AXIS_DATA_WIDTH{1'b0}};
reg [AXIS_KEEP_WIDTH-1:0] temp_m_axis_read_data_tkeep_reg  = {AXIS_KEEP_WIDTH{1'b0}};
reg                       temp_m_axis_read_data_tvalid_reg = 1'b0, temp_m_axis_read_data_tvalid_next;
reg                       temp_m_axis_read_data_tlast_reg  = 1'b0;
reg [AXIS_ID_WIDTH-1:0]   temp_m_axis_read_data_tid_reg    = {AXIS_ID_WIDTH{1'b0}};
reg [AXIS_DEST_WIDTH-1:0] temp_m_axis_read_data_tdest_reg  = {AXIS_DEST_WIDTH{1'b0}};
reg [AXIS_USER_WIDTH-1:0] temp_m_axis_read_data_tuser_reg  = {AXIS_USER_WIDTH{1'b0}};

// datapath control
reg store_axis_int_to_output;
reg store_axis_int_to_temp;
reg store_axis_temp_to_output;

assign m_axis_read_data_tdata  = m_axis_read_data_tdata_reg;
assign m_axis_read_data_tkeep  = AXIS_KEEP_ENABLE ? m_axis_read_data_tkeep_reg : {AXIS_KEEP_WIDTH{1'b1}};
assign m_axis_read_data_tvalid = m_axis_read_data_tvalid_reg;
assign m_axis_read_data_tlast  = AXIS_LAST_ENABLE ? m_axis_read_data_tlast_reg : 1'b1;
assign m_axis_read_data_tid    = AXIS_ID_ENABLE   ? m_axis_read_data_tid_reg   : {AXIS_ID_WIDTH{1'b0}};
assign m_axis_read_data_tdest  = AXIS_DEST_ENABLE ? m_axis_read_data_tdest_reg : {AXIS_DEST_WIDTH{1'b0}};
assign m_axis_read_data_tuser  = AXIS_USER_ENABLE ? m_axis_read_data_tuser_reg : {AXIS_USER_WIDTH{1'b0}};

// enable ready input next cycle if output is ready or the temp reg will not be filled on the next cycle (output reg empty or no input)
assign m_axis_read_data_tready_int_early = m_axis_read_data_tready || (!temp_m_axis_read_data_tvalid_reg && (!m_axis_read_data_tvalid_reg || !m_axis_read_data_tvalid_int));

always @* begin
    // transfer sink ready state to source
    m_axis_read_data_tvalid_next = m_axis_read_data_tvalid_reg;
    temp_m_axis_read_data_tvalid_next = temp_m_axis_read_data_tvalid_reg;

    store_axis_int_to_output = 1'b0;
    store_axis_int_to_temp = 1'b0;
    store_axis_temp_to_output = 1'b0;

    if (m_axis_read_data_tready_int_reg) begin
        // input is ready
        if (m_axis_read_data_tready || !m_axis_read_data_tvalid_reg) begin
            // output is ready or currently not valid, transfer data to output
            m_axis_read_data_tvalid_next = m_axis_read_data_tvalid_int;
            store_axis_int_to_output = 1'b1;
        end else begin
            // output is not ready, store input in temp
            temp_m_axis_read_data_tvalid_next = m_axis_read_data_tvalid_int;
            store_axis_int_to_temp = 1'b1;
        end
    end else if (m_axis_read_data_tready) begin
        // input is not ready, but output is ready
        m_axis_read_data_tvalid_next = temp_m_axis_read_data_tvalid_reg;
        temp_m_axis_read_data_tvalid_next = 1'b0;
        store_axis_temp_to_output = 1'b1;
    end
end

always @(posedge clk) begin
    if (rst) begin
        m_axis_read_data_tvalid_reg <= 1'b0;
        m_axis_read_data_tready_int_reg <= 1'b0;
        temp_m_axis_read_data_tvalid_reg <= 1'b0;
    end else begin
        m_axis_read_data_tvalid_reg <= m_axis_read_data_tvalid_next;
        m_axis_read_data_tready_int_reg <= m_axis_read_data_tready_int_early;
        temp_m_axis_read_data_tvalid_reg <= temp_m_axis_read_data_tvalid_next;
    end

    // datapath
    if (store_axis_int_to_output) begin
        m_axis_read_data_tdata_reg <= m_axis_read_data_tdata_int;
        m_axis_read_data_tkeep_reg <= m_axis_read_data_tkeep_int;
        m_axis_read_data_tlast_reg <= m_axis_read_data_tlast_int;
        m_axis_read_data_tid_reg   <= m_axis_read_data_tid_int;
        m_axis_read_data_tdest_reg <= m_axis_read_data_tdest_int;
        m_axis_read_data_tuser_reg <= m_axis_read_data_tuser_int;
    end else if (store_axis_temp_to_output) begin
        m_axis_read_data_tdata_reg <= temp_m_axis_read_data_tdata_reg;
        m_axis_read_data_tkeep_reg <= temp_m_axis_read_data_tkeep_reg;
        m_axis_read_data_tlast_reg <= temp_m_axis_read_data_tlast_reg;
        m_axis_read_data_tid_reg   <= temp_m_axis_read_data_tid_reg;
        m_axis_read_data_tdest_reg <= temp_m_axis_read_data_tdest_reg;
        m_axis_read_data_tuser_reg <= temp_m_axis_read_data_tuser_reg;
    end

    if (store_axis_int_to_temp) begin
        temp_m_axis_read_data_tdata_reg <= m_axis_read_data_tdata_int;
        temp_m_axis_read_data_tkeep_reg <= m_axis_read_data_tkeep_int;
        temp_m_axis_read_data_tlast_reg <= m_axis_read_data_tlast_int;
        temp_m_axis_read_data_tid_reg   <= m_axis_read_data_tid_int;
        temp_m_axis_read_data_tdest_reg <= m_axis_read_data_tdest_int;
        temp_m_axis_read_data_tuser_reg <= m_axis_read_data_tuser_int;
    end
end

endmodule
/*

Copyright (c) 2018 Alex Forencich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

// Language: Verilog 2001

`timescale 1ns / 1ps

/*
 * AXI4 DMA
 */
module axi_dma_wr #
(
    // Width of AXI data bus in bits
    parameter AXI_DATA_WIDTH = 32,
    // Width of AXI address bus in bits
    parameter AXI_ADDR_WIDTH = 16,
    // Width of AXI wstrb (width of data bus in words)
    parameter AXI_STRB_WIDTH = (AXI_DATA_WIDTH/8),
    // Width of AXI ID signal
    parameter AXI_ID_WIDTH = 8,
    // Maximum AXI burst length to generate
    parameter AXI_MAX_BURST_LEN = 16,
    // Width of AXI stream interfaces in bits
    parameter AXIS_DATA_WIDTH = AXI_DATA_WIDTH,
    // Use AXI stream tkeep signal
    parameter AXIS_KEEP_ENABLE = (AXIS_DATA_WIDTH>8),
    // AXI stream tkeep signal width (words per cycle)
    parameter AXIS_KEEP_WIDTH = (AXIS_DATA_WIDTH/8),
    // Use AXI stream tlast signal
    parameter AXIS_LAST_ENABLE = 1,
    // Propagate AXI stream tid signal
    parameter AXIS_ID_ENABLE = 0,
    // AXI stream tid signal width
    parameter AXIS_ID_WIDTH = 8,
    // Propagate AXI stream tdest signal
    parameter AXIS_DEST_ENABLE = 0,
    // AXI stream tdest signal width
    parameter AXIS_DEST_WIDTH = 8,
    // Propagate AXI stream tuser signal
    parameter AXIS_USER_ENABLE = 1,
    // AXI stream tuser signal width
    parameter AXIS_USER_WIDTH = 1,
    // Width of length field
    parameter LEN_WIDTH = 20,
    // Width of tag field
    parameter TAG_WIDTH = 8,
    // Enable support for scatter/gather DMA
    // (multiple descriptors per AXI stream frame)
    parameter ENABLE_SG = 0,
    // Enable support for unaligned transfers
    parameter ENABLE_UNALIGNED = 0
)
(
    input  wire                       clk,
    input  wire                       rst,

    /*
     * AXI write descriptor input
     */
    input  wire [AXI_ADDR_WIDTH-1:0]  s_axis_write_desc_addr,
    input  wire [LEN_WIDTH-1:0]       s_axis_write_desc_len,
    input  wire [TAG_WIDTH-1:0]       s_axis_write_desc_tag,
    input  wire                       s_axis_write_desc_valid,
    output wire                       s_axis_write_desc_ready,

    /*
     * AXI write descriptor status output
     */
    output wire [LEN_WIDTH-1:0]       m_axis_write_desc_status_len,
    output wire [TAG_WIDTH-1:0]       m_axis_write_desc_status_tag,
    output wire [AXIS_ID_WIDTH-1:0]   m_axis_write_desc_status_id,
    output wire [AXIS_DEST_WIDTH-1:0] m_axis_write_desc_status_dest,
    output wire [AXIS_USER_WIDTH-1:0] m_axis_write_desc_status_user,
    output wire                       m_axis_write_desc_status_valid,

    /*
     * AXI stream write data input
     */
    input  wire [AXIS_DATA_WIDTH-1:0] s_axis_write_data_tdata,
    input  wire [AXIS_KEEP_WIDTH-1:0] s_axis_write_data_tkeep,
    input  wire                       s_axis_write_data_tvalid,
    output wire                       s_axis_write_data_tready,
    input  wire                       s_axis_write_data_tlast,
    input  wire [AXIS_ID_WIDTH-1:0]   s_axis_write_data_tid,
    input  wire [AXIS_DEST_WIDTH-1:0] s_axis_write_data_tdest,
    input  wire [AXIS_USER_WIDTH-1:0] s_axis_write_data_tuser,

    /*
     * AXI master interface
     */
    output wire [AXI_ID_WIDTH-1:0]    m_axi_awid,
    output wire [AXI_ADDR_WIDTH-1:0]  m_axi_awaddr,
    output wire [7:0]                 m_axi_awlen,
    output wire [2:0]                 m_axi_awsize,
    output wire [1:0]                 m_axi_awburst,
    output wire                       m_axi_awlock,
    output wire [3:0]                 m_axi_awcache,
    output wire [2:0]                 m_axi_awprot,
    output wire                       m_axi_awvalid,
    input  wire                       m_axi_awready,
    output wire [AXI_DATA_WIDTH-1:0]  m_axi_wdata,
    output wire [AXI_STRB_WIDTH-1:0]  m_axi_wstrb,
    output wire                       m_axi_wlast,
    output wire                       m_axi_wvalid,
    input  wire                       m_axi_wready,
    input  wire [AXI_ID_WIDTH-1:0]    m_axi_bid,
    input  wire [1:0]                 m_axi_bresp,
    input  wire                       m_axi_bvalid,
    output wire                       m_axi_bready,

    /*
     * Configuration
     */
    input  wire                       enable,
    input  wire                       abort
);

parameter AXI_WORD_WIDTH = AXI_STRB_WIDTH;
parameter AXI_WORD_SIZE = AXI_DATA_WIDTH/AXI_WORD_WIDTH;
parameter AXI_BURST_SIZE = $clog2(AXI_STRB_WIDTH);
parameter AXI_MAX_BURST_SIZE = AXI_MAX_BURST_LEN << AXI_BURST_SIZE;

parameter AXIS_KEEP_WIDTH_INT = AXIS_KEEP_ENABLE ? AXIS_KEEP_WIDTH : 1;
parameter AXIS_WORD_WIDTH = AXIS_KEEP_WIDTH_INT;
parameter AXIS_WORD_SIZE = AXIS_DATA_WIDTH/AXIS_WORD_WIDTH;

parameter OFFSET_WIDTH = AXI_STRB_WIDTH > 1 ? $clog2(AXI_STRB_WIDTH) : 1;
parameter OFFSET_MASK = AXI_STRB_WIDTH > 1 ? {OFFSET_WIDTH{1'b1}} : 0;
parameter ADDR_MASK = {AXI_ADDR_WIDTH{1'b1}} << $clog2(AXI_STRB_WIDTH);
parameter CYCLE_COUNT_WIDTH = LEN_WIDTH - AXI_BURST_SIZE + 1;

parameter STATUS_FIFO_ADDR_WIDTH = 5;

// bus width assertions
// initial begin
//     if (AXI_WORD_SIZE * AXI_STRB_WIDTH != AXI_DATA_WIDTH) begin
//         $error("Error: AXI data width not evenly divisble (instance %m)");
//         $finish;
//     end
// 
//     if (AXIS_WORD_SIZE * AXIS_KEEP_WIDTH_INT != AXIS_DATA_WIDTH) begin
//         $error("Error: AXI stream data width not evenly divisble (instance %m)");
//         $finish;
//     end
// 
//     if (AXI_WORD_SIZE != AXIS_WORD_SIZE) begin
//         $error("Error: word size mismatch (instance %m)");
//         $finish;
//     end
// 
//     if (2**$clog2(AXI_WORD_WIDTH) != AXI_WORD_WIDTH) begin
//         $error("Error: AXI word width must be even power of two (instance %m)");
//         $finish;
//     end
// 
//     if (AXI_DATA_WIDTH != AXIS_DATA_WIDTH) begin
//         $error("Error: AXI interface width must match AXI stream interface width (instance %m)");
//         $finish;
//     end
// 
//     if (AXI_MAX_BURST_LEN < 1 || AXI_MAX_BURST_LEN > 256) begin
//         $error("Error: AXI_MAX_BURST_LEN must be between 1 and 256 (instance %m)");
//         $finish;
//     end
// 
//     if (ENABLE_SG) begin
//         $error("Error: scatter/gather is not yet implemented (instance %m)");
//         $finish;
//     end
// end

localparam [2:0]
    STATE_IDLE = 3'd0,
    STATE_START = 3'd1,
    STATE_WRITE = 3'd2,
    STATE_FINISH_BURST = 3'd3,
    STATE_DROP_DATA = 3'd4;

reg [2:0] state_reg = STATE_IDLE, state_next;

// datapath control signals
reg transfer_in_save;
reg flush_save;
reg status_fifo_we;

integer i;
reg [OFFSET_WIDTH:0] cycle_size;

reg [AXI_ADDR_WIDTH-1:0] addr_reg = {AXI_ADDR_WIDTH{1'b0}}, addr_next;
reg [LEN_WIDTH-1:0] op_word_count_reg = {LEN_WIDTH{1'b0}}, op_word_count_next;
reg [LEN_WIDTH-1:0] tr_word_count_reg = {LEN_WIDTH{1'b0}}, tr_word_count_next;

reg [OFFSET_WIDTH-1:0] offset_reg = {OFFSET_WIDTH{1'b0}}, offset_next;
reg [AXI_STRB_WIDTH-1:0] strb_offset_mask_reg = {AXI_STRB_WIDTH{1'b1}}, strb_offset_mask_next;
reg zero_offset_reg = 1'b1, zero_offset_next;
reg [OFFSET_WIDTH-1:0] last_cycle_offset_reg = {OFFSET_WIDTH{1'b0}}, last_cycle_offset_next;
reg [LEN_WIDTH-1:0] length_reg = {LEN_WIDTH{1'b0}}, length_next;
reg [CYCLE_COUNT_WIDTH-1:0] input_cycle_count_reg = {CYCLE_COUNT_WIDTH{1'b0}}, input_cycle_count_next;
reg [CYCLE_COUNT_WIDTH-1:0] output_cycle_count_reg = {CYCLE_COUNT_WIDTH{1'b0}}, output_cycle_count_next;
reg input_active_reg = 1'b0, input_active_next;
reg first_cycle_reg = 1'b0, first_cycle_next;
reg input_last_cycle_reg = 1'b0, input_last_cycle_next;
reg output_last_cycle_reg = 1'b0, output_last_cycle_next;
reg last_transfer_reg = 1'b0, last_transfer_next;

reg [TAG_WIDTH-1:0] tag_reg = {TAG_WIDTH{1'b0}}, tag_next;
reg [AXIS_ID_WIDTH-1:0] axis_id_reg = {AXIS_ID_WIDTH{1'b0}}, axis_id_next;
reg [AXIS_DEST_WIDTH-1:0] axis_dest_reg = {AXIS_DEST_WIDTH{1'b0}}, axis_dest_next;
reg [AXIS_USER_WIDTH-1:0] axis_user_reg = {AXIS_USER_WIDTH{1'b0}}, axis_user_next;

reg [STATUS_FIFO_ADDR_WIDTH+1-1:0] status_fifo_wr_ptr_reg = 0, status_fifo_wr_ptr_next;
reg [STATUS_FIFO_ADDR_WIDTH+1-1:0] status_fifo_rd_ptr_reg = 0, status_fifo_rd_ptr_next;
reg [LEN_WIDTH-1:0] status_fifo_len[(2**STATUS_FIFO_ADDR_WIDTH)-1:0];
reg [TAG_WIDTH-1:0] status_fifo_tag[(2**STATUS_FIFO_ADDR_WIDTH)-1:0];
reg [AXIS_ID_WIDTH-1:0] status_fifo_id[(2**STATUS_FIFO_ADDR_WIDTH)-1:0];
reg [AXIS_DEST_WIDTH-1:0] status_fifo_dest[(2**STATUS_FIFO_ADDR_WIDTH)-1:0];
reg [AXIS_USER_WIDTH-1:0] status_fifo_user[(2**STATUS_FIFO_ADDR_WIDTH)-1:0];
reg status_fifo_last[(2**STATUS_FIFO_ADDR_WIDTH)-1:0];
reg [LEN_WIDTH-1:0] status_fifo_wr_len;
reg [TAG_WIDTH-1:0] status_fifo_wr_tag;
reg [AXIS_ID_WIDTH-1:0] status_fifo_wr_id;
reg [AXIS_DEST_WIDTH-1:0] status_fifo_wr_dest;
reg [AXIS_USER_WIDTH-1:0] status_fifo_wr_user;
reg status_fifo_wr_last;

reg s_axis_write_desc_ready_reg = 1'b0, s_axis_write_desc_ready_next;

reg [LEN_WIDTH-1:0] m_axis_write_desc_status_len_reg = {LEN_WIDTH{1'b0}}, m_axis_write_desc_status_len_next;
reg [TAG_WIDTH-1:0] m_axis_write_desc_status_tag_reg = {TAG_WIDTH{1'b0}}, m_axis_write_desc_status_tag_next;
reg [AXIS_ID_WIDTH-1:0] m_axis_write_desc_status_id_reg = {AXIS_ID_WIDTH{1'b0}}, m_axis_write_desc_status_id_next;
reg [AXIS_DEST_WIDTH-1:0] m_axis_write_desc_status_dest_reg = {AXIS_DEST_WIDTH{1'b0}}, m_axis_write_desc_status_dest_next;
reg [AXIS_USER_WIDTH-1:0] m_axis_write_desc_status_user_reg = {AXIS_USER_WIDTH{1'b0}}, m_axis_write_desc_status_user_next;
reg m_axis_write_desc_status_valid_reg = 1'b0, m_axis_write_desc_status_valid_next;

reg [AXI_ADDR_WIDTH-1:0] m_axi_awaddr_reg = {AXI_ADDR_WIDTH{1'b0}}, m_axi_awaddr_next;
reg [7:0] m_axi_awlen_reg = 8'd0, m_axi_awlen_next;
reg m_axi_awvalid_reg = 1'b0, m_axi_awvalid_next;
reg m_axi_bready_reg = 1'b0, m_axi_bready_next;

reg s_axis_write_data_tready_reg = 1'b0, s_axis_write_data_tready_next;

reg [AXIS_DATA_WIDTH-1:0] save_axis_tdata_reg = {AXIS_DATA_WIDTH{1'b0}};
reg [AXIS_KEEP_WIDTH_INT-1:0] save_axis_tkeep_reg = {AXIS_KEEP_WIDTH_INT{1'b0}};
reg save_axis_tlast_reg = 1'b0;

reg [AXIS_DATA_WIDTH-1:0] shift_axis_tdata;
reg [AXIS_KEEP_WIDTH_INT-1:0] shift_axis_tkeep;
reg shift_axis_tvalid;
reg shift_axis_tlast;
reg shift_axis_input_tready;
reg shift_axis_extra_cycle_reg = 1'b0;

// internal datapath
reg  [AXI_DATA_WIDTH-1:0] m_axi_wdata_int;
reg  [AXI_STRB_WIDTH-1:0] m_axi_wstrb_int;
reg                       m_axi_wlast_int;
reg                       m_axi_wvalid_int;
reg                       m_axi_wready_int_reg = 1'b0;
wire                      m_axi_wready_int_early;

assign s_axis_write_desc_ready = s_axis_write_desc_ready_reg;

assign m_axis_write_desc_status_len = m_axis_write_desc_status_len_reg;
assign m_axis_write_desc_status_tag = m_axis_write_desc_status_tag_reg;
assign m_axis_write_desc_status_id = m_axis_write_desc_status_id_reg;
assign m_axis_write_desc_status_dest = m_axis_write_desc_status_dest_reg;
assign m_axis_write_desc_status_user = m_axis_write_desc_status_user_reg;
assign m_axis_write_desc_status_valid = m_axis_write_desc_status_valid_reg;

assign s_axis_write_data_tready = s_axis_write_data_tready_reg;

assign m_axi_awid = {AXI_ID_WIDTH{1'b0}};
assign m_axi_awaddr = m_axi_awaddr_reg;
assign m_axi_awlen = m_axi_awlen_reg;
assign m_axi_awsize = AXI_BURST_SIZE;
assign m_axi_awburst = 2'b01;
assign m_axi_awlock = 1'b0;
assign m_axi_awcache = 4'b0011;
assign m_axi_awprot = 3'b010;
assign m_axi_awvalid = m_axi_awvalid_reg;
assign m_axi_bready = m_axi_bready_reg;

wire [AXI_ADDR_WIDTH-1:0] addr_plus_max_burst = addr_reg + AXI_MAX_BURST_SIZE;
wire [AXI_ADDR_WIDTH-1:0] addr_plus_count = addr_reg + op_word_count_reg;

always @* begin
    if (!ENABLE_UNALIGNED || zero_offset_reg) begin
        // passthrough if no overlap
        shift_axis_tdata = s_axis_write_data_tdata;
        shift_axis_tkeep = s_axis_write_data_tkeep;
        shift_axis_tvalid = s_axis_write_data_tvalid;
        shift_axis_tlast = AXIS_LAST_ENABLE && s_axis_write_data_tlast;
        shift_axis_input_tready = 1'b1;
    end else if (!AXIS_LAST_ENABLE) begin
        shift_axis_tdata = {s_axis_write_data_tdata, save_axis_tdata_reg} >> ((AXIS_KEEP_WIDTH_INT-offset_reg)*AXIS_WORD_SIZE);
        shift_axis_tkeep = {s_axis_write_data_tkeep, save_axis_tkeep_reg} >> (AXIS_KEEP_WIDTH_INT-offset_reg);
        shift_axis_tvalid = s_axis_write_data_tvalid;
        shift_axis_tlast = 1'b0;
        shift_axis_input_tready = 1'b1;
    end else if (shift_axis_extra_cycle_reg) begin
        shift_axis_tdata = {s_axis_write_data_tdata, save_axis_tdata_reg} >> ((AXIS_KEEP_WIDTH_INT-offset_reg)*AXIS_WORD_SIZE);
        shift_axis_tkeep = {{AXIS_KEEP_WIDTH_INT{1'b0}}, save_axis_tkeep_reg} >> (AXIS_KEEP_WIDTH_INT-offset_reg);
        shift_axis_tvalid = 1'b1;
        shift_axis_tlast = save_axis_tlast_reg;
        shift_axis_input_tready = flush_save;
    end else begin
        shift_axis_tdata = {s_axis_write_data_tdata, save_axis_tdata_reg} >> ((AXIS_KEEP_WIDTH_INT-offset_reg)*AXIS_WORD_SIZE);
        shift_axis_tkeep = {s_axis_write_data_tkeep, save_axis_tkeep_reg} >> (AXIS_KEEP_WIDTH_INT-offset_reg);
        shift_axis_tvalid = s_axis_write_data_tvalid;
        shift_axis_tlast = (s_axis_write_data_tlast && ((s_axis_write_data_tkeep & ({AXIS_KEEP_WIDTH_INT{1'b1}} << (AXIS_KEEP_WIDTH_INT-offset_reg))) == 0));
        shift_axis_input_tready = 1'b1;
    end
end

always @* begin
    state_next = STATE_IDLE;

    s_axis_write_desc_ready_next = 1'b0;

    m_axis_write_desc_status_len_next = m_axis_write_desc_status_len_reg;
    m_axis_write_desc_status_tag_next = m_axis_write_desc_status_tag_reg;
    m_axis_write_desc_status_id_next = m_axis_write_desc_status_id_reg;
    m_axis_write_desc_status_dest_next = m_axis_write_desc_status_dest_reg;
    m_axis_write_desc_status_user_next = m_axis_write_desc_status_user_reg;
    m_axis_write_desc_status_valid_next = 1'b0;

    s_axis_write_data_tready_next = 1'b0;

    m_axi_awaddr_next = m_axi_awaddr_reg;
    m_axi_awlen_next = m_axi_awlen_reg;
    m_axi_awvalid_next = m_axi_awvalid_reg && !m_axi_awready;
    m_axi_wdata_int = shift_axis_tdata;
    m_axi_wstrb_int = shift_axis_tkeep;
    m_axi_wlast_int = 1'b0;
    m_axi_wvalid_int = 1'b0;
    m_axi_bready_next = 1'b0;

    transfer_in_save = 1'b0;
    flush_save = 1'b0;
    status_fifo_we = 1'b0;

    cycle_size = AXIS_KEEP_WIDTH_INT;

    addr_next = addr_reg;
    offset_next = offset_reg;
    strb_offset_mask_next = strb_offset_mask_reg;
    zero_offset_next = zero_offset_reg;
    last_cycle_offset_next = last_cycle_offset_reg;
    length_next = length_reg;
    op_word_count_next = op_word_count_reg;
    tr_word_count_next = tr_word_count_reg;
    input_cycle_count_next = input_cycle_count_reg;
    output_cycle_count_next = output_cycle_count_reg;
    input_active_next = input_active_reg;
    first_cycle_next = first_cycle_reg;
    input_last_cycle_next = input_last_cycle_reg;
    output_last_cycle_next = output_last_cycle_reg;
    last_transfer_next = last_transfer_reg;

    status_fifo_rd_ptr_next = status_fifo_rd_ptr_reg;

    tag_next = tag_reg;
    axis_id_next = axis_id_reg;
    axis_dest_next = axis_dest_reg;
    axis_user_next = axis_user_reg;

    status_fifo_wr_len = length_reg;
    status_fifo_wr_tag = tag_reg;
    status_fifo_wr_id = axis_id_reg;
    status_fifo_wr_dest = axis_dest_reg;
    status_fifo_wr_user = axis_user_reg;
    status_fifo_wr_last = 1'b0;

    case (state_reg)
        STATE_IDLE: begin
            // idle state - load new descriptor to start operation
            flush_save = 1'b1;
            s_axis_write_desc_ready_next = enable;

            if (ENABLE_UNALIGNED) begin
                addr_next = s_axis_write_desc_addr;
                offset_next = s_axis_write_desc_addr & OFFSET_MASK;
                strb_offset_mask_next = {AXI_STRB_WIDTH{1'b1}} << (s_axis_write_desc_addr & OFFSET_MASK);
                zero_offset_next = (s_axis_write_desc_addr & OFFSET_MASK) == 0;
                last_cycle_offset_next = offset_next + (s_axis_write_desc_len & OFFSET_MASK);
            end else begin
                addr_next = s_axis_write_desc_addr & ADDR_MASK;
                offset_next = 0;
                strb_offset_mask_next = {AXI_STRB_WIDTH{1'b1}};
                zero_offset_next = 1'b1;
                last_cycle_offset_next = offset_next + (s_axis_write_desc_len & OFFSET_MASK);
            end
            tag_next = s_axis_write_desc_tag;
            op_word_count_next = s_axis_write_desc_len;
            first_cycle_next = 1'b1;
            length_next = 0;

            if (s_axis_write_desc_ready && s_axis_write_desc_valid) begin
                s_axis_write_desc_ready_next = 1'b0;
                state_next = STATE_START;
            end else begin
                state_next = STATE_IDLE;
            end
        end
        STATE_START: begin
            // start state - initiate new AXI transfer
            if (op_word_count_reg <= AXI_MAX_BURST_SIZE - (addr_reg & OFFSET_MASK) || AXI_MAX_BURST_SIZE >= 4096) begin
                // packet smaller than max burst size
                if (addr_reg[12] != addr_plus_count[12]) begin
                    // crosses 4k boundary
                    tr_word_count_next = 13'h1000 - addr_reg[11:0];
                end else begin
                    // does not cross 4k boundary
                    tr_word_count_next = op_word_count_reg;
                end
            end else begin
                // packet larger than max burst size
                if (addr_reg[12] != addr_plus_max_burst[12]) begin
                    // crosses 4k boundary
                    tr_word_count_next = 13'h1000 - addr_reg[11:0];
                end else begin
                    // does not cross 4k boundary
                    tr_word_count_next = AXI_MAX_BURST_SIZE - (addr_reg & OFFSET_MASK);
                end
            end

            input_cycle_count_next = (tr_word_count_next - 1) >> $clog2(AXIS_KEEP_WIDTH_INT);
            input_last_cycle_next = input_cycle_count_next == 0;
            if (ENABLE_UNALIGNED) begin
                output_cycle_count_next = (tr_word_count_next + (addr_reg & OFFSET_MASK) - 1) >> AXI_BURST_SIZE;
            end else begin
                output_cycle_count_next = (tr_word_count_next - 1) >> AXI_BURST_SIZE;
            end
            output_last_cycle_next = output_cycle_count_next == 0;
            last_transfer_next = tr_word_count_next == op_word_count_reg;
            input_active_next = 1'b1;

            if (ENABLE_UNALIGNED) begin
                if (!first_cycle_reg && last_transfer_next) begin
                    if (offset_reg >= last_cycle_offset_reg && last_cycle_offset_reg > 0) begin
                        // last cycle will be served by stored partial cycle
                        input_active_next = input_cycle_count_next > 0;
                        input_cycle_count_next = input_cycle_count_next - 1;
                    end
                end
            end

            if (!m_axi_awvalid_reg) begin
                m_axi_awaddr_next = addr_reg;
                m_axi_awlen_next = output_cycle_count_next;
                m_axi_awvalid_next = s_axis_write_data_tvalid || !first_cycle_reg;

                if (m_axi_awvalid_next) begin
                    addr_next = addr_reg + tr_word_count_next;
                    op_word_count_next = op_word_count_reg - tr_word_count_next;

                    s_axis_write_data_tready_next = m_axi_wready_int_early && input_active_next;
                    state_next = STATE_WRITE;
                end else begin
                    state_next = STATE_START;
                end
            end else begin
                state_next = STATE_START;
            end
        end
        STATE_WRITE: begin
            s_axis_write_data_tready_next = m_axi_wready_int_early && (last_transfer_reg || input_active_reg) && shift_axis_input_tready;

            if (m_axi_wready_int_reg && ((s_axis_write_data_tready && shift_axis_tvalid) || (!input_active_reg && !last_transfer_reg) || !shift_axis_input_tready)) begin
                if (s_axis_write_data_tready && s_axis_write_data_tvalid) begin
                    transfer_in_save = 1'b1;

                    axis_id_next = s_axis_write_data_tid;
                    axis_dest_next = s_axis_write_data_tdest;
                    axis_user_next = s_axis_write_data_tuser;
                end

                // update counters
                if (first_cycle_reg) begin
                    length_next = length_reg + (AXIS_KEEP_WIDTH_INT - offset_reg);
                end else begin
                    length_next = length_reg + AXIS_KEEP_WIDTH_INT;
                end
                if (input_active_reg) begin
                    input_cycle_count_next = input_cycle_count_reg - 1;
                    input_active_next = input_cycle_count_reg > 0;
                end
                input_last_cycle_next = input_cycle_count_next == 0;
                output_cycle_count_next = output_cycle_count_reg - 1;
                output_last_cycle_next = output_cycle_count_next == 0;
                first_cycle_next = 1'b0;
                strb_offset_mask_next = {AXI_STRB_WIDTH{1'b1}};

                m_axi_wdata_int = shift_axis_tdata;
                m_axi_wstrb_int = strb_offset_mask_reg;
                m_axi_wvalid_int = 1'b1;

                if (AXIS_LAST_ENABLE && s_axis_write_data_tlast) begin
                    // end of input frame
                    input_active_next = 1'b0;
                    s_axis_write_data_tready_next = 1'b0;
                end

                if (AXIS_LAST_ENABLE && shift_axis_tlast) begin
                    // end of data packet

                    if (AXIS_KEEP_ENABLE) begin
                        cycle_size = AXIS_KEEP_WIDTH_INT;
                        for (i = AXIS_KEEP_WIDTH_INT-1; i >= 0; i = i - 1) begin
                            if (~shift_axis_tkeep & strb_offset_mask_reg & (1 << i)) begin
                                cycle_size = i;
                            end
                        end
                    end else begin
                        cycle_size = AXIS_KEEP_WIDTH_INT;
                    end

                    if (output_last_cycle_reg) begin
                        m_axi_wlast_int = 1'b1;

                        // no more data to transfer, finish operation
                        if (last_transfer_reg && last_cycle_offset_reg > 0) begin
                            if (AXIS_KEEP_ENABLE && !(shift_axis_tkeep & ~({AXI_STRB_WIDTH{1'b1}} >> (AXI_STRB_WIDTH - last_cycle_offset_reg)))) begin
                                m_axi_wstrb_int = strb_offset_mask_reg & shift_axis_tkeep;
                                if (first_cycle_reg) begin
                                    length_next = length_reg + (cycle_size - offset_reg);
                                end else begin
                                    length_next = length_reg + cycle_size;
                                end
                            end else begin
                                m_axi_wstrb_int = strb_offset_mask_reg & {AXI_STRB_WIDTH{1'b1}} >> (AXI_STRB_WIDTH - last_cycle_offset_reg);
                                if (first_cycle_reg) begin
                                    length_next = length_reg + (last_cycle_offset_reg - offset_reg);
                                end else begin
                                    length_next = length_reg + last_cycle_offset_reg;
                                end
                            end
                        end else begin
                            if (AXIS_KEEP_ENABLE) begin
                                m_axi_wstrb_int = strb_offset_mask_reg & shift_axis_tkeep;
                                if (first_cycle_reg) begin
                                    length_next = length_reg + (cycle_size - offset_reg);
                                end else begin
                                    length_next = length_reg + cycle_size;
                                end
                            end
                        end

                        // enqueue status FIFO entry for write completion
                        status_fifo_we = 1'b1;
                        status_fifo_wr_len = length_next;
                        status_fifo_wr_tag = tag_reg;
                        status_fifo_wr_id = axis_id_next;
                        status_fifo_wr_dest = axis_dest_next;
                        status_fifo_wr_user = axis_user_next;
                        status_fifo_wr_last = 1'b1;

                        s_axis_write_data_tready_next = 1'b0;
                        s_axis_write_desc_ready_next = enable;
                        state_next = STATE_IDLE;
                    end else begin
                        // more cycles left in burst, finish burst
                        if (AXIS_KEEP_ENABLE) begin
                            m_axi_wstrb_int = strb_offset_mask_reg & shift_axis_tkeep;
                            if (first_cycle_reg) begin
                                length_next = length_reg + (cycle_size - offset_reg);
                            end else begin
                                length_next = length_reg + cycle_size;
                            end
                        end

                        // enqueue status FIFO entry for write completion
                        status_fifo_we = 1'b1;
                        status_fifo_wr_len = length_next;
                        status_fifo_wr_tag = tag_reg;
                        status_fifo_wr_id = axis_id_next;
                        status_fifo_wr_dest = axis_dest_next;
                        status_fifo_wr_user = axis_user_next;
                        status_fifo_wr_last = 1'b1;

                        s_axis_write_data_tready_next = 1'b0;
                        state_next = STATE_FINISH_BURST;
                    end

                end else if (output_last_cycle_reg) begin
                    m_axi_wlast_int = 1'b1;

                    if (op_word_count_reg > 0) begin
                        // current AXI transfer complete, but there is more data to transfer
                        // enqueue status FIFO entry for write completion
                        status_fifo_we = 1'b1;
                        status_fifo_wr_len = length_next;
                        status_fifo_wr_tag = tag_reg;
                        status_fifo_wr_id = axis_id_next;
                        status_fifo_wr_dest = axis_dest_next;
                        status_fifo_wr_user = axis_user_next;
                        status_fifo_wr_last = 1'b0;

                        s_axis_write_data_tready_next = 1'b0;
                        state_next = STATE_START;
                    end else begin
                        // no more data to transfer, finish operation
                        if (last_cycle_offset_reg > 0) begin
                            m_axi_wstrb_int = strb_offset_mask_reg & {AXI_STRB_WIDTH{1'b1}} >> (AXI_STRB_WIDTH - last_cycle_offset_reg);
                            if (first_cycle_reg) begin
                                length_next = length_reg + (last_cycle_offset_reg - offset_reg);
                            end else begin
                                length_next = length_reg + last_cycle_offset_reg;
                            end
                        end

                        // enqueue status FIFO entry for write completion
                        status_fifo_we = 1'b1;
                        status_fifo_wr_len = length_next;
                        status_fifo_wr_tag = tag_reg;
                        status_fifo_wr_id = axis_id_next;
                        status_fifo_wr_dest = axis_dest_next;
                        status_fifo_wr_user = axis_user_next;
                        status_fifo_wr_last = 1'b1;

                        if (AXIS_LAST_ENABLE) begin
                            // not at the end of packet; drop remainder
                            s_axis_write_data_tready_next = shift_axis_input_tready;
                            state_next = STATE_DROP_DATA;
                        end else begin
                            // no framing; return to idle
                            s_axis_write_data_tready_next = 1'b0;
                            s_axis_write_desc_ready_next = enable;
                            state_next = STATE_IDLE;
                        end
                    end
                end else begin
                    s_axis_write_data_tready_next = m_axi_wready_int_early && (last_transfer_reg || input_active_next) && shift_axis_input_tready;
                    state_next = STATE_WRITE;
                end
            end else begin
                state_next = STATE_WRITE;
            end
        end
        STATE_FINISH_BURST: begin
            // finish current AXI burst

            if (m_axi_wready_int_reg) begin
                // update counters
                if (input_active_reg) begin
                    input_cycle_count_next = input_cycle_count_reg - 1;
                    input_active_next = input_cycle_count_reg > 0;
                end
                input_last_cycle_next = input_cycle_count_next == 0;
                output_cycle_count_next = output_cycle_count_reg - 1;
                output_last_cycle_next = output_cycle_count_next == 0;

                m_axi_wdata_int = {AXI_DATA_WIDTH{1'b0}};
                m_axi_wstrb_int = {AXI_STRB_WIDTH{1'b0}};
                m_axi_wvalid_int = 1'b1;

                if (output_last_cycle_reg) begin
                    // no more data to transfer, finish operation
                    m_axi_wlast_int = 1'b1;

                    s_axis_write_data_tready_next = 1'b0;
                    s_axis_write_desc_ready_next = enable;
                    state_next = STATE_IDLE;
                end else begin
                    // more cycles in AXI transfer
                    state_next = STATE_FINISH_BURST;
                end
            end else begin
                state_next = STATE_FINISH_BURST;
            end
        end
        STATE_DROP_DATA: begin
            // drop excess AXI stream data
            s_axis_write_data_tready_next = shift_axis_input_tready;

            if (shift_axis_tvalid) begin
                if (s_axis_write_data_tready && s_axis_write_data_tvalid) begin
                    transfer_in_save = 1'b1;
                end

                if (shift_axis_tlast) begin
                    s_axis_write_data_tready_next = 1'b0;
                    s_axis_write_desc_ready_next = enable;
                    state_next = STATE_IDLE;
                end else begin
                    state_next = STATE_DROP_DATA;
                end
            end else begin
                state_next = STATE_DROP_DATA;
            end
        end
    endcase

    if (status_fifo_rd_ptr_reg != status_fifo_wr_ptr_reg) begin
        // status FIFO not empty
        if (m_axi_bready && m_axi_bvalid) begin
            // got write completion, pop and return status
            m_axis_write_desc_status_len_next = status_fifo_len[status_fifo_rd_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]];
            m_axis_write_desc_status_tag_next = status_fifo_tag[status_fifo_rd_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]];
            m_axis_write_desc_status_id_next = status_fifo_id[status_fifo_rd_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]];
            m_axis_write_desc_status_dest_next = status_fifo_dest[status_fifo_rd_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]];
            m_axis_write_desc_status_user_next = status_fifo_user[status_fifo_rd_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]];
            m_axis_write_desc_status_valid_next = status_fifo_last[status_fifo_rd_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]];
            status_fifo_rd_ptr_next = status_fifo_rd_ptr_reg + 1;
            m_axi_bready_next = 1'b0;
        end else begin
            // wait for write completion
            m_axi_bready_next = 1'b1;
        end
    end
end

always @(posedge clk) begin
    if (rst) begin
        state_reg <= STATE_IDLE;
        s_axis_write_desc_ready_reg <= 1'b0;
        m_axis_write_desc_status_valid_reg <= 1'b0;
        s_axis_write_data_tready_reg <= 1'b0;
        m_axi_awvalid_reg <= 1'b0;
        m_axi_bready_reg <= 1'b0;
        save_axis_tlast_reg <= 1'b0;
        shift_axis_extra_cycle_reg <= 1'b0;

        status_fifo_wr_ptr_reg <= 0;
        status_fifo_rd_ptr_reg <= 0;
    end else begin
        state_reg <= state_next;
        s_axis_write_desc_ready_reg <= s_axis_write_desc_ready_next;
        m_axis_write_desc_status_valid_reg <= m_axis_write_desc_status_valid_next;
        s_axis_write_data_tready_reg <= s_axis_write_data_tready_next;
        m_axi_awvalid_reg <= m_axi_awvalid_next;
        m_axi_bready_reg <= m_axi_bready_next;

        // datapath
        if (flush_save) begin
            save_axis_tlast_reg <= 1'b0;
            shift_axis_extra_cycle_reg <= 1'b0;
        end else if (transfer_in_save) begin
            save_axis_tlast_reg <= s_axis_write_data_tlast;
            shift_axis_extra_cycle_reg <= s_axis_write_data_tlast & ((s_axis_write_data_tkeep >> (AXIS_KEEP_WIDTH_INT-offset_reg)) != 0);
        end

        if (status_fifo_we) begin
            status_fifo_wr_ptr_reg <= status_fifo_wr_ptr_reg + 1;
        end
        status_fifo_rd_ptr_reg <= status_fifo_rd_ptr_next;
    end

    m_axis_write_desc_status_len_reg <= m_axis_write_desc_status_len_next;
    m_axis_write_desc_status_tag_reg <= m_axis_write_desc_status_tag_next;
    m_axis_write_desc_status_id_reg <= m_axis_write_desc_status_id_next;
    m_axis_write_desc_status_dest_reg <= m_axis_write_desc_status_dest_next;
    m_axis_write_desc_status_user_reg <= m_axis_write_desc_status_user_next;

    m_axi_awaddr_reg <= m_axi_awaddr_next;
    m_axi_awlen_reg <= m_axi_awlen_next;

    addr_reg <= addr_next;
    offset_reg <= offset_next;
    strb_offset_mask_reg <= strb_offset_mask_next;
    zero_offset_reg <= zero_offset_next;
    last_cycle_offset_reg <= last_cycle_offset_next;
    length_reg <= length_next;
    op_word_count_reg <= op_word_count_next;
    tr_word_count_reg <= tr_word_count_next;
    input_cycle_count_reg <= input_cycle_count_next;
    output_cycle_count_reg <= output_cycle_count_next;
    input_active_reg <= input_active_next;
    first_cycle_reg <= first_cycle_next;
    input_last_cycle_reg <= input_last_cycle_next;
    output_last_cycle_reg <= output_last_cycle_next;
    last_transfer_reg <= last_transfer_next;

    tag_reg <= tag_next;
    axis_id_reg <= axis_id_next;
    axis_dest_reg <= axis_dest_next;
    axis_user_reg <= axis_user_next;

    if (flush_save) begin
        save_axis_tkeep_reg <= {AXIS_KEEP_WIDTH_INT{1'b0}};
    end else if (transfer_in_save) begin
        save_axis_tdata_reg <= s_axis_write_data_tdata;
        save_axis_tkeep_reg <= AXIS_KEEP_ENABLE ? s_axis_write_data_tkeep : {AXIS_KEEP_WIDTH_INT{1'b1}};
    end

    if (status_fifo_we) begin
        status_fifo_len[status_fifo_wr_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]] <= status_fifo_wr_len;
        status_fifo_tag[status_fifo_wr_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]] <= status_fifo_wr_tag;
        status_fifo_id[status_fifo_wr_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]] <= status_fifo_wr_id;
        status_fifo_dest[status_fifo_wr_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]] <= status_fifo_wr_dest;
        status_fifo_user[status_fifo_wr_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]] <= status_fifo_wr_user;
        status_fifo_last[status_fifo_wr_ptr_reg[STATUS_FIFO_ADDR_WIDTH-1:0]] <= status_fifo_wr_last;
        status_fifo_wr_ptr_reg <= status_fifo_wr_ptr_reg + 1;
    end
end

// output datapath logic
reg [AXI_DATA_WIDTH-1:0] m_axi_wdata_reg  = {AXI_DATA_WIDTH{1'b0}};
reg [AXI_STRB_WIDTH-1:0] m_axi_wstrb_reg  = {AXI_STRB_WIDTH{1'b0}};
reg                      m_axi_wlast_reg  = 1'b0;
reg                      m_axi_wvalid_reg = 1'b0, m_axi_wvalid_next;

reg [AXI_DATA_WIDTH-1:0] temp_m_axi_wdata_reg  = {AXI_DATA_WIDTH{1'b0}};
reg [AXI_STRB_WIDTH-1:0] temp_m_axi_wstrb_reg  = {AXI_STRB_WIDTH{1'b0}};
reg                      temp_m_axi_wlast_reg  = 1'b0;
reg                      temp_m_axi_wvalid_reg = 1'b0, temp_m_axi_wvalid_next;

// datapath control
reg store_axi_w_int_to_output;
reg store_axi_w_int_to_temp;
reg store_axi_w_temp_to_output;

assign m_axi_wdata  = m_axi_wdata_reg;
assign m_axi_wstrb  = m_axi_wstrb_reg;
assign m_axi_wvalid = m_axi_wvalid_reg;
assign m_axi_wlast  = m_axi_wlast_reg;

// enable ready input next cycle if output is ready or the temp reg will not be filled on the next cycle (output reg empty or no input)
assign m_axi_wready_int_early = m_axi_wready || (!temp_m_axi_wvalid_reg && (!m_axi_wvalid_reg || !m_axi_wvalid_int));

always @* begin
    // transfer sink ready state to source
    m_axi_wvalid_next = m_axi_wvalid_reg;
    temp_m_axi_wvalid_next = temp_m_axi_wvalid_reg;

    store_axi_w_int_to_output = 1'b0;
    store_axi_w_int_to_temp = 1'b0;
    store_axi_w_temp_to_output = 1'b0;

    if (m_axi_wready_int_reg) begin
        // input is ready
        if (m_axi_wready || !m_axi_wvalid_reg) begin
            // output is ready or currently not valid, transfer data to output
            m_axi_wvalid_next = m_axi_wvalid_int;
            store_axi_w_int_to_output = 1'b1;
        end else begin
            // output is not ready, store input in temp
            temp_m_axi_wvalid_next = m_axi_wvalid_int;
            store_axi_w_int_to_temp = 1'b1;
        end
    end else if (m_axi_wready) begin
        // input is not ready, but output is ready
        m_axi_wvalid_next = temp_m_axi_wvalid_reg;
        temp_m_axi_wvalid_next = 1'b0;
        store_axi_w_temp_to_output = 1'b1;
    end
end

always @(posedge clk) begin
    if (rst) begin
        m_axi_wvalid_reg <= 1'b0;
        m_axi_wready_int_reg <= 1'b0;
        temp_m_axi_wvalid_reg <= 1'b0;
    end else begin
        m_axi_wvalid_reg <= m_axi_wvalid_next;
        m_axi_wready_int_reg <= m_axi_wready_int_early;
        temp_m_axi_wvalid_reg <= temp_m_axi_wvalid_next;
    end

    // datapath
    if (store_axi_w_int_to_output) begin
        m_axi_wdata_reg <= m_axi_wdata_int;
        m_axi_wstrb_reg <= m_axi_wstrb_int;
        m_axi_wlast_reg <= m_axi_wlast_int;
    end else if (store_axi_w_temp_to_output) begin
        m_axi_wdata_reg <= temp_m_axi_wdata_reg;
        m_axi_wstrb_reg <= temp_m_axi_wstrb_reg;
        m_axi_wlast_reg <= temp_m_axi_wlast_reg;
    end

    if (store_axi_w_int_to_temp) begin
        temp_m_axi_wdata_reg <= m_axi_wdata_int;
        temp_m_axi_wstrb_reg <= m_axi_wstrb_int;
        temp_m_axi_wlast_reg <= m_axi_wlast_int;
    end
end

endmodule

