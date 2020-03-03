package wuklab

import spinal.core._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config}
import spinal.lib._
import spinal.lib.fsm._
import Utils._
import spinal.core.internals.Operator

trait HeaderProcessor {
  def dataWidth : Int
  def headerWidth : Int

  def dataBytes = dataWidth / 8
  def headerBytes = headerWidth / 8
  def offsetWidth = dataWidth - headerWidth
  def offsetBytes = offsetWidth / 8

  // TODO: check these requires
  require (headerWidth <= dataWidth)
  require (headerWidth % 8 == 0, "Header should be aligned to byte bound")
  require (dataWidth % 8 == 0, "Data should be aligned to byte bound")
}

class PacketParser[B <: Bundle with Header[B]](
                                                val dataWidth : Int, val headerWidth : Int, header : B
                                              ) extends Component with HeaderProcessor {

  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val dataOut = master Stream Fragment(Bits(dataWidth bits))
    val headerOut = master Stream cloneOf(header)
  }

  // if size == 0
  // if (size & (datawidth-1)) <= headerWidth, we will save a cycle -> skipLast
  val headerIn = header.fromWiderBits(io.dataIn.fragment)
  // check reg next when bypass
  val size = RegNextWhenBypass(headerIn.getSize, io.dataIn.firstFire)
  val skipLast = size.maskBy(dataBytes) <= headerBytes

  val (toHeader, toData) = StreamFork2(io.dataIn)
  io.headerOut << toHeader.takeWhen(toHeader.isFirst).fmap (_.fragment |> header.fromWiderBits)

  val offset = RegNextWhen (toData.fragment(offsetWidth-1 downto 0), toData.fire)
  val outData = offset ## toData.fragment(dataWidth-1 downto offsetWidth)
  // delay the data for one cycle
  // TODO: check this
  // We throw the delayed flow.
  val delayedData = toData.stage()

  // if no size, we do not read the data
  val outDataStream = delayedData.haltWhen(size === 0).throwWhen(delayedData.isLast && skipLast)
  io.dataOut.translateFrom (outDataStream) {(out, in) =>
    out.fragment := outData
    out.last := in.last
  }

}

class PacketBuilder[B <: Bundle with Header[B]] (
                                                  val dataWidth : Int, val headerWidth : Int, header : B
                                                ) extends Component with HeaderProcessor {
  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val headerIn = slave Stream cloneOf(header)
    val dataOut = master Stream Fragment(Bits(dataWidth bits))
  }

  val size = RegNextWhenBypass(io.headerIn.getSize, io.headerIn.fire)
  val insertLast = (size & dataBytes.toMask.asUInt) >= offsetBytes

  val dataOut = io.dataIn.shiftAt(headerWidth)
  io.dataOut << io.dataIn.translateWithLastInsert(dataOut, insertLast)
}

class LegoMemEndPointPacketBuilder (implicit config : CoreMemConfig) extends Component with HeaderProcessor {

  override def dataWidth = 512
  override def headerWidth = LegoMemHeader.headerWidth

  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val headerIn = slave Stream LegoMemHeader()
    val dataOut = master Stream Fragment(AxiStreamPayload(config.epAxisConfig))
  }

  val headerIn = io.headerIn.queue(2)

  // Generate this information at the 1st cycle
  val size = RegNextWhenBypass(headerIn.getSize, headerIn.fire)
  val insertLast = size.maskBy(dataBytes) >= offsetBytes

  // When to pop this header?
  // 1st cycle: need the header.
  // last cycle:
  // Go back to state machine!
  // val validHeader = io.headerIn.valid
  headerIn.ready := io.dataIn.lastFire
  val dataIn = io.dataIn.continueWhen(headerIn.valid)

  val (next, dest) = headerIn.stackPop
  val destQueue = ReturnStream(dest, headerIn.fire).queueLowLatency(8)

  val dataOut = dataIn.shiftAt(headerWidth, next.asBits)
  val outStream = dataIn.translateWithLastInsert(dataOut, insertLast)

  // 1st cycle: header + first half of datea.

  io.dataOut << LegoMemEndPoint.mergeDest(outStream, destQueue)
}

// ReqStatus
// accept : header, generate: data mover cmd / header
class MemoryAccessUnit(implicit config : CoreMemConfig) extends Component {

  val dataCmdType = HardType(DataMoverCmd(config.physicalAddrWidth, true))
  val io = new Bundle {
    // interface from request
    val lookupRes = slave Stream config.pteResType
    val headerIn  = slave Stream LegoMemAccessHeader(config.virtualAddrWidth)
    val headerOut = master Stream LegoMemHeader()

    // Interface to the data mover
    val wrCmd = master Stream AxiStreamDMAWriteCommand(config.physicalAddrWidth, config.dmaLengthWidth)
    val rdCmd = master Stream AxiStreamDMAReadCommand(config.dmaAxisConfig, config.physicalAddrWidth, config.dmaLengthWidth)
  }

  type JoinType = Pair[PageTableEntry, LegoMemAccessHeader]
  val joinStream = io.lookupRes >*< io.headerIn

  // cmd path
  def assignFromJoin(cmd : AxiStreamDMACommand, p : JoinType): Unit = {
    cmd.len := p.snd.header.size
    cmd.addr := config.va2pa(p.fst, p.snd.addr)
  }
  // This assume that, there are not wrong command forwarded here

  // First generate status info here
  val parsed = cloneOf(joinStream)
  parsed.translateFrom (joinStream) { (dst, src) =>
    dst := src
    dst.allowOverride
    dst.snd.header.reqStatus := 0
  }

  val (cmd, rsp) = StreamFork2(parsed)


  // We Generate the response first;
  val Seq(wrJoin, rdJoin) = StreamDemux(cmd.takeBy(_.snd.header.reqStatus === 0), cmd.snd.header.reqType(0).asUInt, 2)
  io.wrCmd.translateFrom (wrJoin) (assignFromJoin)
  io.rdCmd.translateFrom (rdJoin) (assignFromJoin)

  // TODO: join with the status returned by data mover
  io.headerOut << rsp.fmap(_.snd.header)

}

// At core memory, we assign sequence number (locally) and do packet paser, do send the lookup request.
// we analyze the header, and do a dispatch at the end of a sequencer

class CoreMemory(implicit config : CoreMemConfig) extends Component {

  val destWidth = 4

  // Assign data for mover
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epAxisConfig)
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }
  }

  val packetParser = new PacketParser(512, LegoMemHeader.headerWidth, LegoMemAccessHeader(config.virtualAddrWidth))
  val packetBuilder = new LegoMemEndPointPacketBuilder
  packetParser.io.dataIn << io.ep.dataIn.liftStream(_.tdata)
  packetBuilder.io.dataOut >> io.ep.dataOut

  val (lookupHeader, accessHeader) = StreamFork2(packetParser.io.headerOut)

  // TODO: generate req here
  val lookup = new AddressLookupUnit
  lookup.io.bus <> io.bus.lookup
  lookup.io.ctrl.in <> io.ep.ctrlIn
  lookup.io.ctrl.out <> io.ep.ctrlOut
  lookup.io.req << lookupHeader.fmap (generateRequest)

  val access = new MemoryAccessUnit
  lookup.io.res <> access.io.lookupRes
  accessHeader <> access.io.headerIn
  packetBuilder.io.headerIn <> access.io.headerOut

  val dma = new AxiStreamDMA(config.dmaAxisConfig, config.accessAxi4Config, config.physicalAddrWidth, config.dmaLengthWidth)
  dma.io.m_axi <> io.bus.access
  dma.io.m_axis_read_data.liftStream(_.tdata) >> packetBuilder.io.dataIn
  // TODO: We need to filter this flow. throw when is not done
  dma.io.s_axis_write_data << AxiStream(config.dmaAxisConfig, packetParser.io.dataOut)
  dma.io.s_axis_read_desc <> access.io.rdCmd
  dma.io.s_axis_write_desc <> access.io.wrCmd

  def generateRequest(header : LegoMemAccessHeader) : AddressLookupRequest = {
    val next = AddressLookupRequest(config.tagWidth)
    next.tag := config.genTag(header.header.pid, header.addr).asUInt
    // TODO: fix this
    next.seqId := header.header.seqId.resize(16 bits)
    next.reqType := header.header.reqType.resize(2 bits)
    next
  }
}

// For the interconnection, we do not need to add the names into the fifo.

// TODO: look at upsize converter
// data keep last
// class CoreCtrl
// class Sequencer
// We need a downsizer for this!

// We need A => A function, we have =>