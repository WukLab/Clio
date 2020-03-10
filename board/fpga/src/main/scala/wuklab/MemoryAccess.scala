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
                                                val dataWidth : Int, header : B
                                              ) extends Component with HeaderProcessor {

  override def headerWidth = header.packedWidth

  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val dataOut = master Stream Fragment(Bits(dataWidth bits))
    val headerOut = master Stream cloneOf(header)
  }

  // if size == 0
  // if (size & (datawidth-1)) <= headerWidth, we will save a cycle -> skipLast
  val headerIn = header.fromWiderBits(io.dataIn.fragment)
  // check reg next when bypass
  val size = RegNextWhen(headerIn.getSize, io.dataIn.firstFire)
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
  // TODO: check this condition
  val outDataStream = delayedData.haltWhen(size === 0).throwWhen(!delayedData.isFirst && delayedData.isLast && skipLast)
  io.dataOut.translateFrom (outDataStream) {(out, in) =>
    out.fragment := outData
    out.last := in.last
  }

}

class PacketBuilder[B <: Bundle with Header[B]] (
                                                  val dataWidth : Int, header : B
                                                ) extends Component with HeaderProcessor {
  override def headerWidth = header.packedWidth

  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val headerIn = slave Stream cloneOf(header)
    val dataOut = master Stream Fragment(Bits(dataWidth bits))
  }

  io.headerIn.ready := io.dataIn.lastFire
  val insertLast = io.headerIn.getSize.maskBy(dataBytes) >= offsetBytes

  val dataOut = io.dataIn.shiftAt(header.packedWidth, io.headerIn.asBits)
  io.dataOut << io.dataIn.translateWithLastInsert(dataOut, insertLast)
}

class LegoMemEndPointPacketBuilder (implicit config : CoreMemConfig) extends Component with HeaderProcessor {

  override def dataWidth = 512
  override def headerWidth = LegoMemHeader.headerWidth

  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val headerIn = slave Stream LegoMemHeader()
    val dataOut = master Stream Fragment(AxiStreamPayload(config.epDataAxisConfig))
  }

  io.headerIn.ready := io.dataIn.lastFire
  // Generate this information at the 1st cycle
  val (next, dest) = io.headerIn.stackPop
  val insertLast = io.headerIn.getSize.maskBy(dataBytes) >= offsetBytes

  val dataOut = io.dataIn.shiftAt(headerWidth, next.asBits)
  val outStream = io.dataIn.translateWithLastInsert(dataOut, insertLast)

  // 1st cycle: header + first half of data.
  io.dataOut.translateFrom (outStream) { (axis, data) =>
    axis.last := data.last
    axis.fragment.tdata := data.fragment
    axis.fragment.tdest := dest
  }
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

    // Singals for input data
    val wrDataSignal = master Stream Bool
  }

  // TODO: check delay
  type JoinType = Pair[PageTableEntry, LegoMemAccessHeader]
  val joinStream = (io.lookupRes >*< io.headerIn).stage()

  // cmd path
  def assignFromJoin(cmd : AxiStreamDMACommand, p : JoinType): Unit = {
    cmd.len := p.snd.header.size
    cmd.addr := config.va2pa(p.fst, p.snd.addr)
  }
  // This assume that, there are not wrong command forwarded here

  // First generate status info here
  val requestChecker = new Area {
    val parsed = cloneOf(joinStream)
    parsed.translateFrom (joinStream) { (dst, src) =>
      dst := src
      dst.allowOverride
      // Build the resp code
      // TODO: make this a function
      val retCode = src.snd.header.reqType.mux(
        0 -> U(0, 4 bits),
        default -> U(0, 4 bits)
      )
      dst.snd.header.reqStatus := retCode
    }

  }

  val (cmd, rsp) = StreamFork2(requestChecker.parsed)

  // We Generate the response first;
  val Seq(wrJoin, rdJoin) = StreamDemux(cmd, cmd.snd.header.reqType(0).asUInt, 2)
  val (wrCmd, wrValid) = StreamFork2(wrJoin)
  io.wrCmd.translateFrom (wrCmd.takeBy(_.snd.header.reqStatus === 0)) (assignFromJoin)
  io.wrDataSignal.translateFrom (wrValid) ( _ := _.snd.header.reqStatus === 0)

  io.rdCmd.translateFrom (rdJoin.takeBy(_.snd.header.reqStatus === 0)) (assignFromJoin)

  // TODO: join with the status returned by data mover
  io.headerOut << rsp.fmap(_.snd.header)

  // Protocol Checking
  def checkRequest(req : JoinType) : UInt = {
    val ret = U(0, 4 bits)
    switch (req.snd.header.reqType) {
      is () {
        ret := 0
      }
    }
    ret
  }
}

// At core memory, we assign sequence number (locally) and do packet paser, do send the lookup request.
// we analyze the header, and do a dispatch at the end of a sequencer

class MemoryAccessEndPoint(implicit config : CoreMemConfig) extends Component {

  val destWidth = 4

  // Assign data for mover
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig)
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }
  }

  val endpoint = new RawInterfaceEndpoint
  endpoint.io.ep <> io.ep

  val packetParser = new PacketParser(512, LegoMemAccessHeader(config.virtualAddrWidth))
  val packetBuilder = new PacketBuilder(512, LegoMemHeader(config.virtualAddrWidth))
  packetParser.io.dataIn << endpoint.io.raw.dataIn
  packetBuilder.io.dataOut >> endpoint.io.raw.dataOut

  // TODO: add fifo here
  val (lookupHeader, accessHeader) = StreamFork2(packetParser.io.headerOut)

  // TODO: generate req here
  val lookup = new AddressLookupUnit
  lookup.io.bus <> io.bus.lookup
  lookup.io.ctrl.in << endpoint.io.raw.ctrlIn
  lookup.io.ctrl.out >> endpoint.io.raw.ctrlOut
  lookup.io.req << lookupHeader.queueLowLatency(32).fmap(generateRequest)

  val access = new MemoryAccessUnit
  lookup.io.res <> access.io.lookupRes
  accessHeader.queue(32) >> access.io.headerIn
  packetBuilder.io.headerIn << access.io.headerOut

  val dma = new axi_dma(config.dmaAxisConfig, config.accessAxi4Config, config.physicalAddrWidth, config.dmaLengthWidth)
  dma.io.m_axi <> io.bus.access
  dma.io.m_axis_read_data.liftStream(_.tdata) >> packetBuilder.io.dataIn
  // TODO: We need to filter this flow. throw when is not done
  val filteredData = packetParser.io.dataOut.filterBySignal(access.io.wrDataSignal)
  dma.io.s_axis_write_data << AxiStream(config.dmaAxisConfig, filteredData)
  dma.io.s_axis_read_desc <> access.io.rdCmd
  dma.io.s_axis_write_desc <> access.io.wrCmd

  dma.io.write_enable := True
  dma.io.read_enable := True
  dma.io.write_abort := False

  def generateRequest(header : LegoMemAccessHeader) : AddressLookupRequest = {
    val next = AddressLookupRequest(config.tagWidth)
    next.tag := config.genTag(header.header.pid, header.addr).asUInt
    // TODO: fix this, use an unified ID
    next.seqId := header.header.seqId.resize(16 bits)
    next.reqType := header.header.reqType.resize(2 bits)
    next
  }
}

// For the interconnection, we do not need to add the names into the fifo.

class RawInterfaceEndpoint(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    // TODO: correct this
    val ep = new LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val raw = new Bundle {
      val dataIn = master Stream Fragment(Bits(512 bits))
      val dataOut = slave Stream Fragment(Bits(512 bits))
      val ctrlIn = master Stream ControlRequest()
      val ctrlOut = slave Stream ControlRequest()
    }
  }

  // Ctrl FIFOs
  io.raw.ctrlIn.translateFrom (io.ep.ctrlIn) { (ctrl, axis) => ctrl.assignFromBits(axis.tdata) }
  io.ep.ctrlOut.translateFrom (io.raw.ctrlOut) { (axis, ctrl) => axis.tdata := ctrl.asBits; axis.tdest := ctrl.epid }

  // Data FIFOs
  io.raw.dataIn.translateFrom (io.ep.dataIn) { (f, s) => f.last := s.last; f.fragment := s.fragment.tdata }
  val (header, dest) = LegoMemHeader(io.raw.dataOut.fragment).stackPop
  val destReg = RegNextWhenBypass(dest, io.raw.dataOut.firstFire)
  io.ep.dataOut.translateFrom (io.raw.dataOut) { (s, f) =>
    s.last := f.last
    s.fragment.tdata := f.fragment
    s.fragment.tdest := destReg
  }
  // We need to overwrite the first
  // TODO: Still not that, smooth
  when (io.raw.dataIn.isFirst) { io.ep.dataOut.tdata(512 downBy LegoMemHeader.headerWidth) := header.asBits }
}
