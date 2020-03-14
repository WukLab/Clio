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

  val offset = RegNextWhen (toData.fragment(dataWidth-1 downto headerWidth), toData.fire)
  val outData = toData.fragment(dataWidth-1 downto offsetWidth) ## offset
  // delay the data for one cycle
  // TODO: check this
  // We throw the delayed flow.
  // This is the only "register" in the module, should block newer flow
  // So the size should be fine
  val delayedData = toData.stage()

  // if no size, we do not read the data
  // TODO: check this condition
  val outDataStream = delayedData
                        .throwWhen(size <= header.packedBytes)
                        .throwWhen(!delayedData.isFirst && delayedData.isLast && skipLast)
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

  val noData = io.headerIn.getSize <= header.packedBytes
  val dataOut = io.dataIn.shiftAt(header.packedWidth, io.headerIn.payload.asBits)

  // With data path
  val insertLast = io.headerIn.getSize.maskBy(dataBytes) >= offsetBytes
  val withDataStream = io.dataIn.translateWithLastInsert(dataOut, insertLast)

  // no data path
  val noDataStream = cloneOf(io.dataOut)
  noDataStream.fragment := dataOut
  noDataStream.last := True
  noDataStream.valid := io.headerIn.valid

  // Merge
  io.headerIn.ready := Mux(noData, noDataStream.fire, io.dataIn.lastFire)
  io.dataOut << noData.select(withDataStream, noDataStream)
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

  import LegoMem._

  // TODO: check delay
  type JoinType = Pair[PageTableEntry, LegoMemAccessHeader]
  val joinStream = (io.lookupRes >*< io.headerIn).stage()

  // cmd path
  def assignFromJoin(cmd : AxiStreamDMACommand, p : JoinType): Unit = {
    cmd.len := p.snd.length(15 downto 0)
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
      dst.snd.header.reqType := LegoMem.RequestType.resp(src.snd.header.reqType)
      dst.snd.header.reqStatus := checkRequest(src)
      dst.snd.header.size := genSize(src)
    }

  }

  val (cmd, rsp) = StreamFork2(requestChecker.parsed)

  // We Generate the response first;
  // We Filter if the request is valid later
  val Seq(rdJoin, wrJoin) = StreamDemux2(cmd, cmd.snd.header.reqType === RequestType.WRITE_RESP)
  val (wrCmd, wrValid) = StreamFork2(wrJoin)
  io.wrCmd.translateFrom (wrCmd.takeBy(_.snd.header.reqStatus === RequestStatus.OKAY)) (assignFromJoin)
  io.wrDataSignal.translateFrom (wrValid) ( _ := _.snd.header.reqStatus === RequestStatus.OKAY)

  io.rdCmd.translateFrom (rdJoin.takeBy(_.snd.header.reqStatus === RequestStatus.OKAY)) (assignFromJoin)

  // TODO: join with the status returned by data mover
  io.headerOut << rsp.fmap(_.snd.header)

  // Protocol Checking
  def checkRequest(req : JoinType) : UInt = {
    val ret = UInt(4 bits)
    switch (req.snd.header.reqType) {

      is (LegoMem.RequestType.WRITE) {
        ret := LegoMem.RequestStatus.OKAY
      }

      is (LegoMem.RequestType.READ) {
        ret := LegoMem.RequestStatus.OKAY
      }

      default {
        ret := LegoMem.RequestStatus.ERR_INVALID
      }
    }
    ret
  }

  def genSize(req : JoinType) : UInt = {
    val size = UInt(16 bits)
    switch (req.snd.header.reqType) {
      is (LegoMem.RequestType.READ) {
        size := req.snd.length(15 downto 0) + 8
      }

      default {
        size := 8
      }
    }
    size
  }
}

// At core memory, we assign sequence number (locally) and do packet paser, do send the lookup request.
// we analyze the header, and do a dispatch at the end of a sequencer

class MemoryAccessEndPoint(implicit config : CoreMemConfig) extends Component {

  val destWidth = 4
  val headerQueueSize = 32
  val dataQueueSize = 1024

  // Assign data for mover
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }
  }

  val endpoint = new RawInterfaceEndpoint
  endpoint.io.ep <> io.ep

  val packetParser = new PacketParser(512, LegoMemAccessHeader(config.virtualAddrWidth))
  val packetBuilder = new PacketBuilder(512, LegoMemHeader())
  packetParser.io.dataIn << endpoint.io.raw.dataIn
  packetBuilder.io.dataOut >> endpoint.io.raw.dataOut

  // TODO: add fifo here
  val (lookupHeader, accessHeader) = StreamFork2(packetParser.io.headerOut)

  // TODO: generate req here
  val lookup = new AddressLookupUnit
  lookup.io.bus <> io.bus.lookup
  lookup.io.ctrl.in << endpoint.io.raw.ctrlIn
  lookup.io.ctrl.out >> endpoint.io.raw.ctrlOut
  lookup.io.req << lookupHeader.queueLowLatency(headerQueueSize).fmap(generateRequest)

  val access = new MemoryAccessUnit
  lookup.io.res <> access.io.lookupRes
  accessHeader.queue(headerQueueSize) >> access.io.headerIn
  packetBuilder.io.headerIn << access.io.headerOut

  val dma = new axi_dma(config.dmaAxisConfig, config.accessAxi4Config, config.physicalAddrWidth, config.dmaLengthWidth)
  dma.io.m_axi <> io.bus.access
  dma.io.m_axis_read_data.liftStream(_.tdata) >> packetBuilder.io.dataIn
  // TODO: split the defination of queue
  val filteredData = packetParser.io.dataOut.queue(dataQueueSize).filterBySignal(access.io.wrDataSignal)
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
    next.reqType := AddressLookupRequest.RequestType.LOOKUP
    next
  }
}

// For the interconnection, we do not need to add the names into the fifo.

case class LegoMemRawInterface() extends Bundle with IMasterSlave {
  val dataIn = Stream Fragment(Bits(512 bits))
  val dataOut = Stream Fragment(Bits(512 bits))
  val ctrlIn = Stream (ControlRequest())
  val ctrlOut = Stream (ControlRequest())

  override def asMaster(): Unit = {
    master (dataIn)
    slave (dataOut)
    master (ctrlIn)
    slave (ctrlOut)
  }
}

class RawInterfaceEndpoint(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    // TODO: correct this
    val ep = new LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val raw = master (LegoMemRawInterface())
  }

  // Ctrl FIFOs
  io.raw.ctrlIn.translateFrom (io.ep.ctrlIn) { (ctrl, axis) => ctrl.assignFromBits(axis.tdata) }
  io.ep.ctrlOut.translateFrom (io.raw.ctrlOut) { (axis, ctrl) => axis.tdata := ctrl.asBits; axis.tdest := ctrl.epid.resize(4) }

  // Data FIFOs
  io.raw.dataIn.translateFrom (io.ep.dataIn) { (f, s) => f.last := s.last; f.fragment := s.fragment.tdata }
  val (header, dest) = LegoMemHeader(io.raw.dataOut.fragment).stackPop
  val destReg = RegNextWhenBypass(dest, io.raw.dataOut.isFirst)
  io.ep.dataOut.translateFrom (io.raw.dataOut) { (s, f) =>
    s.last := f.last
    s.fragment.tdata := f.fragment
    s.fragment.tdest := destReg
  }
  // We need to overwrite the first
  // TODO: Still not that, smooth
  when (io.raw.dataIn.isFirst) { io.ep.dataOut.tdata(512 downBy LegoMemHeader.headerWidth) := header.asBits }
}
