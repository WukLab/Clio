package wuklab

import spinal.core._
import spinal.lib._

import Utils._

// This module converts a streaming interface to LegoMem Requests
// Also this one will provide
class LegoMemDMA(dmaConfig: AxiStreamDMAConfig, virtualAddrWidth : Int) extends Component {
  val io = new Bundle {
    val lego = slave (LegoMemRawDataInterface())
    val dma = slave (AxiStreamDMAInterface(dmaConfig))

    // Configurations
    val regs = new Bundle {
      val pid = in UInt(16 bits)
      // TODO: fix this width
      val baseAddr = in UInt(virtualAddrWidth bits)
    }
  }

  def accessHeader = LegoMemAccessHeader(virtualAddrWidth)
  val counterIncr = Bool
  val counter = Counter(8 bits, counterIncr)

  val readHeader = Stream(accessHeader).translateFrom (io.dma.read.cmd) { (header, cmd) =>
    buildDMAHeader(header)
    header.header.pid     := io.regs.pid
    header.header.reqType := LegoMem.RequestType.READ
    header.header.size    := header.packedBytes
    header.addr   := cmd.addr + io.regs.baseAddr
    header.length := cmd.len.resized
  }
  val writeHeader = Stream(accessHeader).translateFrom (io.dma.write.cmd) { (header, cmd) =>
    buildDMAHeader(header)
    header.header.pid     := io.regs.pid
    header.header.reqType := LegoMem.RequestType.WRITE
    header.header.size    := header.packedBytes + cmd.len
    header.addr   := cmd.addr + io.regs.baseAddr
    header.length := cmd.len.resized
  }

  // Since the request is inorder, we do not have to reorder the info
  val packetBuilder = new PacketBuilder(512, LegoMemAccessHeader(virtualAddrWidth))
  val selectedHeader = StreamArbiterFactory
    .lowerFirst
    .onArgs(readHeader, writeHeader)
    .changeBy(_.header.seqId := counter.value)
  counterIncr := selectedHeader.fire
  packetBuilder.io.headerIn <-< selectedHeader
  packetBuilder.io.dataIn <-< io.dma.write.data
  packetBuilder.io.dataOut >> io.lego.dataOut

  // We ignore the replies, all read reply goes back to the DMA read
  val packetParser = new PacketParser(512, LegoMemAccessHeader(virtualAddrWidth))
  packetParser.io.headerOut.freeRun()
  packetParser.io.dataOut >-> io.dma.read.data
  packetParser.io.dataIn << io.lego.dataIn

  def buildDMAHeader(header : LegoMemAccessHeader) : Unit = {
    header.header.tag       := 0
    header.header.reqStatus := 0
    header.header.flagRoute := False
    header.header.flagRepl  := False
    header.header.rsvd      := 0
    header.header.seqId     := 0

    header.header.cont    := LegoMem.Continuation(LegoMem.Continuation.EP_COREMEM, LegoMem.Continuation.EP_KEYVALUE)
    header.header.srcPort   := 0
    header.header.destPort  := 0
    header.header.destIp    := 0
  }
}
