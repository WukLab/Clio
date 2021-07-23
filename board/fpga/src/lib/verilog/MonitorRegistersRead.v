// Generator : SpinalHDL v1.3.8    git head : 57d97088b91271a094cebad32ed86479199955df
// Date      : 19/05/2020, 03:36:13
// Component : MonitorRegisters


module MonitorRegistersRead (
      input  [31:0] inputRegs_0,
      input  [31:0] inputRegs_1,
      input  [31:0] inputRegs_2,
      input  [31:0] inputRegs_3,
      input  [31:0] inputRegs_4,
      input  [31:0] inputRegs_5,
      input  [31:0] inputRegs_6,
      input  [31:0] inputRegs_7,
      input  [31:0] inputRegs_8,
      input  [31:0] inputRegs_9,
      input  [31:0] inputRegs_10,
      input  [31:0] inputRegs_11,
      input  [31:0] inputRegs_12,
      input  [31:0] inputRegs_13,
      input  [31:0] inputRegs_14,
      input  [31:0] inputRegs_15,
      input   regBus_awvalid,
      output  regBus_awready,
      input  [31:0] regBus_awaddr,
      input  [2:0] regBus_awprot,
      input   regBus_wvalid,
      output  regBus_wready,
      input  [31:0] regBus_wdata,
      input  [3:0] regBus_wstrb,
      output  regBus_bvalid,
      input   regBus_bready,
      output [1:0] regBus_bresp,
      input   regBus_arvalid,
      output  regBus_arready,
      input  [31:0] regBus_araddr,
      input  [2:0] regBus_arprot,
      output  regBus_rvalid,
      input   regBus_rready,
      output [31:0] regBus_rdata,
      output [1:0] regBus_rresp,
      input   clk,
      input   resetn);
  wire  regs_readHaltRequest;
  wire  regs_writeHaltRequest;
  wire  regs_writeJoinEvent_valid;
  wire  regs_writeJoinEvent_ready;
  wire  _zz_1_;
  wire [1:0] regs_writeRsp_resp;
  wire  regs_writeJoinEvent_translated_valid;
  wire  regs_writeJoinEvent_translated_ready;
  wire [1:0] regs_writeJoinEvent_translated_payload_resp;
  wire  _zz_2_;
  wire  _zz_3_;
  wire  _zz_4_;
  reg  _zz_5_;
  reg [1:0] _zz_6_;
  wire  regs_readDataStage_valid;
  wire  regs_readDataStage_ready;
  wire [31:0] regs_readDataStage_payload_addr;
  wire [2:0] regs_readDataStage_payload_prot;
  reg  regBus_ar_m2sPipe_rValid;
  reg [31:0] regBus_ar_m2sPipe_rData_addr;
  reg [2:0] regBus_ar_m2sPipe_rData_prot;
  reg [31:0] regs_readRsp_data;
  wire [1:0] regs_readRsp_resp;
  wire  _zz_7_;
  wire  regs_writeOccur;
  wire  regs_readOccur;
  assign regs_readHaltRequest = 1'b0;
  assign regs_writeHaltRequest = 1'b0;
  assign _zz_1_ = (regs_writeJoinEvent_valid && regs_writeJoinEvent_ready);
  assign regs_writeJoinEvent_valid = (regBus_awvalid && regBus_wvalid);
  assign regBus_awready = _zz_1_;
  assign regBus_wready = _zz_1_;
  assign regs_writeJoinEvent_translated_valid = regs_writeJoinEvent_valid;
  assign regs_writeJoinEvent_ready = regs_writeJoinEvent_translated_ready;
  assign regs_writeJoinEvent_translated_payload_resp = regs_writeRsp_resp;
  assign _zz_2_ = (! regs_writeHaltRequest);
  assign regs_writeJoinEvent_translated_ready = (_zz_3_ && _zz_2_);
  assign _zz_3_ = ((1'b1 && (! _zz_4_)) || regBus_bready);
  assign _zz_4_ = _zz_5_;
  assign regBus_bvalid = _zz_4_;
  assign regBus_bresp = _zz_6_;
  assign regBus_arready = ((1'b1 && (! regs_readDataStage_valid)) || regs_readDataStage_ready);
  assign regs_readDataStage_valid = regBus_ar_m2sPipe_rValid;
  assign regs_readDataStage_payload_addr = regBus_ar_m2sPipe_rData_addr;
  assign regs_readDataStage_payload_prot = regBus_ar_m2sPipe_rData_prot;
  assign _zz_7_ = (! regs_readHaltRequest);
  assign regs_readDataStage_ready = (regBus_rready && _zz_7_);
  assign regBus_rvalid = (regs_readDataStage_valid && _zz_7_);
  assign regBus_rdata = regs_readRsp_data;
  assign regBus_rresp = regs_readRsp_resp;
  assign regs_writeRsp_resp = (2'b00);
  assign regs_readRsp_resp = (2'b00);
  always @ (*) begin
    regs_readRsp_data = (32'b00000000000000000000000000000000);
    case(regs_readDataStage_payload_addr)
      32'b10100000000000001101000000000000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_0;
      end
      32'b10100000000000001101000000000100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_1;
      end
      32'b10100000000000001101000000001000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_2;
      end
      32'b10100000000000001101000000001100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_3;
      end
      32'b10100000000000001101000000010000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_4;
      end
      32'b10100000000000001101000000010100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_5;
      end
      32'b10100000000000001101000000011000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_6;
      end
      32'b10100000000000001101000000011100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_7;
      end
      32'b10100000000000001101000000100000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_8;
      end
      32'b10100000000000001101000000100100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_9;
      end
      32'b10100000000000001101000000101000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_10;
      end
      32'b10100000000000001101000000101100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_11;
      end
      32'b10100000000000001101000000110000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_12;
      end
      32'b10100000000000001101000000110100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_13;
      end
      32'b10100000000000001101000000111000 : begin
        regs_readRsp_data[31 : 0] = inputRegs_14;
      end
      32'b10100000000000001101000000111100 : begin
        regs_readRsp_data[31 : 0] = inputRegs_15;
      end
      default : begin
      end
    endcase
  end

  assign regs_writeOccur = (regs_writeJoinEvent_valid && regs_writeJoinEvent_ready);
  assign regs_readOccur = (regBus_rvalid && regBus_rready);
  always @ (posedge clk) begin
    if(!resetn) begin
      _zz_5_ <= 1'b0;
      regBus_ar_m2sPipe_rValid <= 1'b0;
    end else begin
      if(_zz_3_)begin
        _zz_5_ <= (regs_writeJoinEvent_translated_valid && _zz_2_);
      end
      if(regBus_arready)begin
        regBus_ar_m2sPipe_rValid <= regBus_arvalid;
      end
    end
  end

  always @ (posedge clk) begin
    if(_zz_3_)begin
      _zz_6_ <= regs_writeJoinEvent_translated_payload_resp;
    end
    if(regBus_arready)begin
      regBus_ar_m2sPipe_rData_addr <= regBus_araddr;
      regBus_ar_m2sPipe_rData_prot <= regBus_arprot;
    end
  end

endmodule


