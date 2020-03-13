package wuklab

import spinal.core._
import spinal.lib._
import Utils._
import spinal.lib.bus.amba4.axi._

case class AxiStreamConfig( dataWidth : Int,
                            keepWidth : Int = 0,
                            idWidth : Int = 0,
                            destWidth : Int = 0,
                            userWidth : Int = 0
                          ) {
  def useKeep = keepWidth > 0
  def useId = idWidth > 0
  def useDest = destWidth > 0
  def useUser = userWidth > 0
}

object AxiStreamConfig {
  def LegoMemEndPointConfig(destWidth : Int) = AxiStreamConfig(512, destWidth = destWidth)
}

case class AxiStreamPayload(config : AxiStreamConfig) extends Bundle {
  assert(config.dataWidth % 8 == 0, "Data is not byte aligned in Axi Stream Interface")
  if (config.useKeep)
    assert(config.dataWidth / 8 == config.keepWidth, "keep width mismatch with data")

  val tdata = Bits(config.dataWidth bits)
  val tdest = if (config.useDest) UInt(config.destWidth bits) else null
  val tkeep = if (config.useKeep) Bits(config.keepWidth bits) else null
  val tuser = if (config.useUser) Bits(config.userWidth bits) else null
  val tid   = if (config.useId)   Bits(config.idWidth bits)   else null
}

object AxiStream {
  def apply(config : AxiStreamConfig, bitStream : Stream[Fragment[Bits]]) : Stream[Fragment[AxiStreamPayload]] = {
    val stream = Stream Fragment AxiStreamPayload(config)
    stream.translateFrom (bitStream) { (axis, bits) =>
      axis.last := bits.last
      axis.fragment.tdata := bits.fragment
    }
    stream
  }
}

object LegoMemEndPoint {
  type LegoMemEndPointStream = Stream[Fragment[AxiStreamPayload]]
}

case class LegoMemEndPoint(dataConfig : AxiStreamConfig, ctrlConfig : AxiStreamConfig) extends Bundle {
  val dataOut = master Stream Fragment(AxiStreamPayload(dataConfig))
  val dataIn  = slave  Stream Fragment(AxiStreamPayload(dataConfig))
  val ctrlOut = master Stream AxiStreamPayload(ctrlConfig)
  val ctrlIn  = slave  Stream AxiStreamPayload(ctrlConfig)
}

// Converter
class LegoMemDataOutputAdapter(externalWidth : Int) extends Component {
  assert(512 % externalWidth == 0, "Adapter: width conversion is not 1:n")
  // this one implies outputWidth is pow of 2

  val adapterConfig = AxiStreamConfig(512, keepWidth = 64)
  val externalConfig = AxiStreamConfig(externalWidth, keepWidth = externalWidth / 8)

  val io = new Bundle {
    val internal = new Bundle {
      val dataIn = slave Stream Fragment(Bits(512 bits))
      val dataOut = master Stream Fragment(Bits(512 bits))
    }
    val external = new Bundle {
      val dataIn = slave Stream Fragment(AxiStreamPayload(externalConfig))
      val dataOut = master Stream Fragment(AxiStreamPayload(externalConfig))
    }
  }

  // convert internal to converter
  val adaptCtrl = new Area {
    val dataOut = Stream Fragment AxiStreamPayload(adapterConfig)

    val size = RegNextWhenBypass(LegoMemHeader(io.internal.dataIn.fragment).size, io.internal.dataIn.isFirst)
    // TODO: check this
    val header = LegoMemHeader(io.internal.dataIn.fragment)
    val lastKeep = ((U(1, 64 bits) << header.getSize.maskBy(64)) - 1).resize(64).asBits

    dataOut.translateFrom (io.internal.dataIn) { (next, beat) =>
      next.last := beat.last
      next.fragment.tdata := beat.fragment
      next.fragment.tkeep := Mux(beat.last, lastKeep, Bits(64 bits).setAll())
    }

  }

  // Connect this to a converter
  // TODO: add in and out fifos
  val inPipe = new AxiStreamWidthConverter(adapterConfig, externalConfig)
  inPipe.io.s_axis << io.external.dataIn
  inPipe.io.m_axis.liftStream(_.tdata) >> io.internal.dataOut
  val outPipe = new AxiStreamWidthConverter(externalConfig, adapterConfig)
  outPipe.io.s_axis << adaptCtrl.dataOut
  outPipe.io.m_axis >> io.external.dataOut

}

class AxiStreamWidthConverter(mconfig : AxiStreamConfig, sconfig: AxiStreamConfig)
  extends BlackBox with XilinxAXI4Toplevel {

  require(mconfig.useKeep, "Master Interface must enable tKeep!")
  require(sconfig.useKeep, "Slave Interface enable tKeep!")

  val generic = new Generic {
    // Width of input AXI stream interface in bits
    val S_DATA_WIDTH = sconfig.dataWidth
    // Propagate tkeep signal on input interface
    // If disabled, tkeep assumed to be 1'b1
    // val S_KEEP_ENABLE = if (sconfig.useKeep) 1 else 0
    // tkeep signal width (words per cycle) on input interface
    // val S_KEEP_WIDTH = (S_DATA_WIDTH/8)
    // Width of output AXI stream interface in bits
    val M_DATA_WIDTH = mconfig.dataWidth
    // Propagate tkeep signal on output interface
    // If disabled, tkeep assumed to be 1'b1
    // val M_KEEP_ENABLE = (M_DATA_WIDTH>8)
    // tkeep signal width (words per cycle) on output interface
    // val M_KEEP_WIDTH = (M_DATA_WIDTH/8)
    // Propagate tid signal
    val ID_ENABLE = 0
    // tid signal width
    val ID_WIDTH = 8
    // Propagate tdest signal
    val DEST_ENABLE = 0
    // tdest signal width
    val DEST_WIDTH = 8
    // Propagate tuser signal
    val USER_ENABLE = 0
    // tuser signal width
    val USER_WIDTH = 1
  }

  val io = new Bundle {
    val clk = in Bool
    val rstn = in Bool

    val m_axis = master Stream Fragment(AxiStreamPayload(mconfig))
    val s_axis = slave Stream Fragment(AxiStreamPayload(sconfig))
  }

  setDefinitionName("axis_adapter")
  addPrePopTask(renameIO)

  mapClockDomain(clock = io.clk, reset = io.rstn)
  addRTLPath("src/lib/verilog/verilog-axis/rtl/axis_adapter.v")
}

class axi_dma(
               axisConfig : AxiStreamConfig, axiConfig : Axi4Config, addrWidth: Int, lenWidth : Int, tagWidth : Int = 0
             ) extends BlackBox with XilinxAXI4Toplevel {
  // TODO: check these width
  val generic = new Generic {
    // Width of AXI data bus in bits
    val AXI_DATA_WIDTH = axisConfig.dataWidth
    // Width of AXI address bus in bits
    val AXI_ADDR_WIDTH = addrWidth
    // Width of AXI wstrb (width of data bus in words)
    // val AXI_STRB_WIDTH = (AXI_DATA_WIDTH/8)
    // Width of AXI ID signal
    val AXI_ID_WIDTH = if (axiConfig.useId) axiConfig.idWidth else 1
    // Maximum AXI burst length to generate
    // This parameter is mainly about the max burst length
    val AXI_MAX_BURST_LEN = 16
    // Width of AXI stream interfaces in bits
    // val AXIS_DATA_WIDTH = AXI_DATA_WIDTH
    // Use AXI stream tkeep signal
    val AXIS_KEEP_ENABLE = if (axisConfig.useKeep) 1 else 0
    // AXI stream tkeep signal width (words per cycle)
    val AXIS_KEEP_WIDTH = if (axisConfig.useKeep) axisConfig.keepWidth else 1
    // Use AXI stream tlast signal
    val AXIS_LAST_ENABLE = 1
    // Propagate AXI stream tid signal
    val AXIS_ID_ENABLE = if (axisConfig.useId) 1 else 0
    // AXI stream tid signal width
    val AXIS_ID_WIDTH = if (axisConfig.useId) axisConfig.idWidth else 1
    // Propagate AXI stream tdest signal
    val AXIS_DEST_ENABLE = if (axisConfig.useDest) 1 else 0
    // AXI stream tdest signal width
    val AXIS_DEST_WIDTH = if (axisConfig.useDest) axisConfig.destWidth else 1
    // Propagate AXI stream tuser signal
    val AXIS_USER_ENABLE = if (axisConfig.useUser) 1 else 0
    // AXI stream tuser signal width
    val AXIS_USER_WIDTH = if (axisConfig.useUser) axisConfig.useUser else 1
    // Width of length field
    val LEN_WIDTH = lenWidth
    // Width of tag field
    val TAG_WIDTH = if (tagWidth > 0) tagWidth else 1
    // Enable support for scatter/gather DMA
    // (multiple descriptors per AXI stream frame)
    val ENABLE_SG = 0
    // Enable support for unaligned transfers
    val ENABLE_UNALIGNED = 1
  }

  val io = new Bundle {
    val clk = in Bool
    val rstn = in Bool

    val s_axis_read_desc  = slave Stream AxiStreamDMAReadCommand(axisConfig, addrWidth, lenWidth, tagWidth)
    val s_axis_write_desc = slave Stream AxiStreamDMAWriteCommand(addrWidth, lenWidth, tagWidth)
    val m_axis_read_data  = master Stream Fragment(AxiStreamPayload(axisConfig))
    val s_axis_write_data = slave Stream Fragment(AxiStreamPayload(axisConfig))
    // TODO: We ignore the datas
    // val m_axis_write_desc_status_id = master Flow NoData
    // val m_axis_write_desc_status_id = master Flow NoData
    val m_axi = master (Axi4(axiConfig))

    val read_enable = in Bool
    val write_enable = in Bool
    val write_abort = in Bool
  }

  setDefinitionName("axi_dma")
  addPrePopTask(renameIO)
  // TODO: check this! the inverse reset
  mapCurrentClockDomain(io.clk, io.rstn)

  addRTLPath("src/lib/verilog/verilog-axi/rtl/axi_dma.v")
  addRTLPath("src/lib/verilog/verilog-axi/rtl/axi_dma_rd.v")
  addRTLPath("src/lib/verilog/verilog-axi/rtl/axi_dma_wr.v")
}

//input  wire [AXI_ADDR_WIDTH-1:0]  s_axis_read_desc_addr,
//input  wire [LEN_WIDTH-1:0]       s_axis_read_desc_len,
//input  wire [TAG_WIDTH-1:0]       s_axis_read_desc_tag,
//input  wire [AXIS_ID_WIDTH-1:0]   s_axis_read_desc_id,
//input  wire [AXIS_DEST_WIDTH-1:0] s_axis_read_desc_dest,
//input  wire [AXIS_USER_WIDTH-1:0] s_axis_read_desc_user,
//input  wire                       s_axis_read_desc_valid,
//output wire                       s_axis_read_desc_ready,

class AxiStreamDMACommand(addrWidth: Int, lenWidth : Int, tagWidth : Int = 0) extends Bundle {
  def useTag = tagWidth > 0

  val addr = UInt(addrWidth bits)
  val len  = UInt(lenWidth bits)
  val tag  = if (useTag) UInt(tagWidth bits) else null
}

case class AxiStreamDMAReadCommand(config : AxiStreamConfig,
                                   addrWidth: Int,
                                   lenWidth : Int,
                                   tagWidth : Int = 0) extends AxiStreamDMACommand(addrWidth, lenWidth, tagWidth) {
  val id   = if (config.useId)   UInt(config.idWidth bits)   else null
  val dest = if (config.useDest) UInt(config.destWidth bits) else null
  val user = if (config.useUser) UInt(config.userWidth bits) else null
}

///*
// * AXI write descriptor input
// */
//input  wire [AXI_ADDR_WIDTH-1:0]  s_axis_write_desc_addr,
//input  wire [LEN_WIDTH-1:0]       s_axis_write_desc_len,
//input  wire [TAG_WIDTH-1:0]       s_axis_write_desc_tag,
//input  wire                       s_axis_write_desc_valid,
//output wire                       s_axis_write_desc_ready,
//
case class AxiStreamDMAWriteCommand(
                                     addrWidth: Int, lenWidth : Int, tagWidth : Int = 0
                                   ) extends AxiStreamDMACommand(addrWidth, lenWidth, tagWidth) {}



// For now, we ignore the status things


//
///*
// * AXI read descriptor status output
// */
//output wire [TAG_WIDTH-1:0]       m_axis_read_desc_status_tag,
//output wire                       m_axis_read_desc_status_valid,
//
///*
// * AXI stream read data output
// */
//output wire [AXIS_DATA_WIDTH-1:0] m_axis_read_data_tdata,
//output wire [AXIS_KEEP_WIDTH-1:0] m_axis_read_data_tkeep,
//output wire                       m_axis_read_data_tvalid,
//input  wire                       m_axis_read_data_tready,
//output wire                       m_axis_read_data_tlast,
//output wire [AXIS_ID_WIDTH-1:0]   m_axis_read_data_tid,
//output wire [AXIS_DEST_WIDTH-1:0] m_axis_read_data_tdest,
//output wire [AXIS_USER_WIDTH-1:0] m_axis_read_data_tuser,
//
///*
// * AXI write descriptor status output
// */
//output wire [LEN_WIDTH-1:0]       m_axis_write_desc_status_len,
//output wire [TAG_WIDTH-1:0]       m_axis_write_desc_status_tag,
//output wire [AXIS_ID_WIDTH-1:0]   m_axis_write_desc_status_id,
//output wire [AXIS_DEST_WIDTH-1:0] m_axis_write_desc_status_dest,
//output wire [AXIS_USER_WIDTH-1:0] m_axis_write_desc_status_user,