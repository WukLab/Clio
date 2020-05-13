package wuklab

import spinal.core._
import spinal.lib._

import Utils._

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

// The complex interface
case class AxiStreamDMAReadInterface(config : AxiStreamConfig, addrWidth: Int, lenWidth : Int, tagWidth : Int = 0) extends Bundle with IMasterSlave {
  val cmd = Stream (AxiStreamDMAReadCommand(config, addrWidth, lenWidth, tagWidth))
  val data = Stream Fragment (Bits(512 bits))

  override def asMaster(): Unit = {
    master (cmd)
    slave (data)
  }
}

case class AxiStreamDMAWriteInterface(addrWidth: Int, lenWidth : Int, tagWidth : Int = 0) extends Bundle with IMasterSlave {
  val cmd = Stream (AxiStreamDMAWriteCommand(addrWidth, lenWidth, tagWidth))
  val data = Stream Fragment (Bits(512 bits))

  override def asMaster(): Unit = {
    master (cmd)
    master (data)
  }
}

case class AxiStreamDMAInterface(config : AxiStreamConfig,
                                 addrWidth: Int,
                                 lenWidth : Int,
                                 tagWidth : Int = 0
                                ) extends Bundle with IMasterSlave {
  val read = AxiStreamDMAReadInterface(config, addrWidth, lenWidth, tagWidth)
  val write = AxiStreamDMAWriteInterface(addrWidth, lenWidth, tagWidth)

  override def asMaster(): Unit = {
    master (read)
    master (write)
  }
}

// DMA Utilities
class AxiStreamDMAReadArbiter(num : Int) extends Component {
  val io = new Bundle {
    val reads = Vec(slave (AxiStreamDMAReadInterface()), num)
    val dma = master (AxiStreamDMAReadInterface())
  }

  val arbiter = StreamArbiterFactory.roundRobin.build(io.dma.cmd.payload, num)
  (arbiter.io.inputs, io.reads).zipped map { _ << _.cmd }
  val (cmdOut, selectStreamF) = StreamFork2(arbiter.io.output)
  io.dma.cmd << cmdOut
  val selectStream = selectStreamF.translateWith (arbiter.io.chosen)

  val dataOuts = io.dma.data.demux(selectStream, num)
  (dataOuts, io.reads).zipped map { _ >> _.data }
}

class AxiStreamDMAWriteArbiter(num : Int) extends Component {
  val io = new Bundle {
    val writes = Vec(slave (AxiStreamDMAWriteInterface()), num)
    val dma = master (AxiStreamDMAWriteInterface())
  }

  val (cmd, data) = StreamWithFragmentArbiter.onInterface(io.writes : _ *) { ifc => (ifc.cmd, ifc.data) }
  io.dma.cmd << cmd
  io.dma.data << data
}

