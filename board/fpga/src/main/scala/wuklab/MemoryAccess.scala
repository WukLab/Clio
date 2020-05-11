package wuklab

import spinal.core._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config}
import spinal.lib._
import spinal.lib.fsm._
import Utils._
import spinal.core.internals.Operator
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Config}

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

  val (toHeader, toDataF) = StreamFork2(io.dataIn)
  // 1. easy path: header
  io.headerOut << toHeader.takeFirst.toStreamOfFragment.fmap (_ |> header.fromWiderBits)

  // 2. data
  // 2.1 filter out no data headers
  val toData = toDataF.throwBy {f => toDataF.first && header.fromWiderBits(f.fragment).getSize <= headerBytes }
  // 2.2 generate data using a state machine
  val headerIn = header.fromWiderBits(toData.fragment)

  // Cycle one, we fill in reg

  val regValid = Reg (Bool) init False
  val size = Reg (cloneOf(headerIn.getSize))
  val lastCycleNoNeedData = size <= offsetBytes && size =/= 0

  val lastCycle = size <= dataBytes
  val dataReg = RegNextWhen(toData.fragment(dataWidth-1 downto headerWidth), toData.fire)
  val outData = toData.fragment(headerWidth-1 downto 0) ## dataReg

  io.dataOut.fragment := outData
  io.dataOut.last := lastCycle

  toData.ready := False
  io.dataOut.valid := False
  // Let compiler optimize this shit
  when (!regValid) {
    // We need to fillin the reg!
    toData.ready := True

    size := headerIn.getSize - headerBytes

    when (toData.valid) {
      when (headerIn.getSize <= dataBytes && io.dataOut.ready) {
        io.dataOut.valid := True
      } otherwise {
        regValid := True
      }
    }
  } otherwise {
    // We can emit data
    // From here, the size IS pure data size

    when (lastCycle) {

      when (lastCycleNoNeedData) {
        io.dataOut.valid := True
        when (io.dataOut.ready) {
          regValid := False
          // TODO: optimization here, can be revoke
          // TODO: DO NOT WAIT TO ENTER STATE 0
          toData.ready := True
          size := headerIn.getSize - headerBytes

          // We can only do register here, can not directly send
          when (toData.valid) {
            regValid := True
          }
        }
      } elsewhen (toData.valid) {
        io.dataOut.valid := True
        when (io.dataOut.ready) {
          toData.ready := True
          regValid := False
        }
      }

    } elsewhen (toData.valid) {
      io.dataOut.valid := True

      when (io.dataOut.ready) {
        toData.ready := True
        size := size - dataBytes
      }
    }
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

  val remainSize = cloneOf(io.headerIn.getSize)
  val remainSizeReg = RegNextWhen(remainSize - dataBytes, io.dataOut.fire)
  remainSize := Mux(io.dataOut.first, io.headerIn.getSize, remainSizeReg)

  // Status regs
  val lastCycle = remainSize <= dataBytes
  val firstCycle = io.dataOut.first

  val lastCycleBytes = io.headerIn.getSize.maskBy(dataBytes)
  val lastCycleUseReg = lastCycleBytes <= headerBytes && lastCycleBytes =/= 0

  val tailReg = RegNextWhen(io.dataIn.fragment(dataWidth - 1 downto offsetWidth), io.dataIn.fire)
  val dataOut =
    io.dataIn.fragment(offsetWidth - 1 downto 0) ## Mux(io.dataOut.first, io.headerIn.payload.asBits, tailReg)

  val lastCycleNoData = (lastCycle && lastCycleUseReg)
  val validData : Bool = io.dataIn.valid || lastCycleNoData

  io.dataOut.fragment := dataOut
  io.dataOut.last := lastCycle
  io.dataOut.valid := io.headerIn.valid && validData

  io.headerIn.ready := io.dataOut.last && io.dataOut.ready && validData
  io.dataIn.ready := io.headerIn.valid && io.dataOut.ready && !lastCycleNoData

}

// ReqStatus
// accept : header, generate: data mover cmd / header
class MemoryAccessUnit(implicit config : CoreMemConfig) extends Component {

  val dataCmdType = HardType(DataMoverCmd(config.physicalAddrWidth, true))
  val io = new Bundle {
    // interface from request
    val lookupRes = slave Stream config.pteResType
    val headerIn  = slave Stream LegoMemAccessHeader(config.virtualAddrWidth)
    val headerOut = master Stream LegoMemAccessHeader(config.virtualAddrWidth)

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
      val status = checkRequest(src)
      dst.snd.header.reqStatus := status
      dst.snd.header.size := genSize(src, status)
    }

  }

  val (cmd, rsp) = StreamFork2(requestChecker.parsed)

  // We Generate the response first;
  // We Filter if the request is valid later
  // TODO: seems this one will process MIG, since ret of MIG is WIRTE
  val Seq(rdJoin, wrJoin) = StreamDemux2(cmd, cmd.snd.header.reqType === RequestType.WRITE_RESP)
  val (wrCmd, wrValid) = StreamFork2(wrJoin)
  io.wrCmd.translateFrom (wrCmd.takeBy(_.snd.header.reqStatus === RequestStatus.OKAY)) (assignFromJoin)
  io.wrDataSignal.translateFrom (wrValid) ( _ := _.snd.header.reqStatus === RequestStatus.OKAY)

  io.rdCmd.translateFrom (rdJoin.takeBy(_.snd.header.reqStatus === RequestStatus.OKAY)) (assignFromJoin)

  // TODO: join with the status returned by data mover
  io.headerOut << rsp.fmap(_.snd)

  // Protocol Checking
  def checkRequest(req : JoinType) : UInt = {
    val ret = UInt(4 bits)
    when (req.fst.used) {
      switch (req.snd.header.reqType) {

        is (LegoMem.RequestType.WRITE, LegoMem.RequestType.READ, LegoMem.RequestType.READ_MIG) {
          ret := LegoMem.RequestStatus.OKAY
        }

        default {
          ret := LegoMem.RequestStatus.ERR_INVALID
        }
      }
    } otherwise {
      ret := LegoMem.RequestStatus.ERR_INVALID
    }
    ret
  }

  def genSize(req : JoinType, status : UInt) : UInt = {
    val size = UInt(16 bits)
    size := req.snd.packedBytes

    when (status === RequestStatus.OKAY) {
      switch (req.snd.header.reqType) {
        is (LegoMem.RequestType.READ, LegoMem.RequestType.READ_MIG) {
          size := req.snd.length(15 downto 0) + req.snd.packedBytes
        }
      }
    }
    size
  }
}

// At core memory, we assign sequence number (locally) and do packet paser, do send the lookup request.
// we analyze the header, and do a dispatch at the end of a sequencer

class MemoryAccessEndPoint(implicit config : CoreMemConfig) extends Component {
  // TODO: commit FIFO here

  val destWidth = 4
  val headerQueueSize = 64
  val writeDataQueueSize = 1024
  val readDataQueueSize = 256
  val dmaRequestQueueSize = 256

  // Assign data for mover
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }

    val regBus = new AxiLite4(AxiLite4Config(32, 32))
  }

  val endpoint = new RawInterfaceEndpoint
  endpoint.io.ep <> io.ep

  val packetParser = new PacketParser(512, LegoMemAccessHeader(config.virtualAddrWidth))
  val packetBuilder = new PacketBuilder(512, LegoMemAccessHeader(config.virtualAddrWidth))
  packetParser.io.dataIn << endpoint.io.raw.dataIn
  packetBuilder.io.dataOut >> endpoint.io.raw.dataOut

  // TODO: add fifo here
  val (lookupHeader, accessHeader) = StreamFork2(packetParser.io.headerOut)

  // TODO: generate req here
  val lookup = new AddressLookupUnit
  lookup.io.bus <> io.bus.lookup
  lookup.io.ctrl.in << endpoint.io.raw.ctrlIn
  lookup.io.ctrl.out >> endpoint.io.raw.ctrlOut
  lookup.io.req << lookupHeader.queueLowLatency(headerQueueSize).fmap(generateLookupRequest)

  val access = new MemoryAccessUnit
  lookup.io.res <> access.io.lookupRes
  accessHeader.takeBy(isValidAccessRequest).queue(headerQueueSize) >> access.io.headerIn
  packetBuilder.io.headerIn << access.io.headerOut.queue(headerQueueSize)

  val dma = new axi_dma(config.dmaAxisConfig, config.accessAxi4Config, config.physicalAddrWidth, config.dmaLengthWidth)
  dma.io.m_axi <> io.bus.access
  dma.io.m_axis_read_data.liftStream(_.tdata).queue(readDataQueueSize) >> packetBuilder.io.dataIn
  // TODO: add a loopback path here, for extended module parameters
  // signal A -> loopback queue -> signal B -> access unit
  val filteredDataF = packetParser.io.dataOut.queue(writeDataQueueSize)
  val filteredData = filteredDataF.filterBySignal(access.io.wrDataSignal)
  dma.io.s_axis_write_data << AxiStream(config.dmaAxisConfig, filteredData)
  dma.io.s_axis_read_desc <> access.io.rdCmd.queue(dmaRequestQueueSize)
  dma.io.s_axis_write_desc <> access.io.wrCmd.queue(dmaRequestQueueSize)

  dma.io.write_enable := True
  dma.io.read_enable := True
  dma.io.write_abort := False

  def generateLookupRequest(header : LegoMemAccessHeader) : AddressLookupRequest = {
    val next = AddressLookupRequest(config.tagWidth)
    // TODO: Fix this, use different page sizes
    // Currently we only fetch the smallest page sizes
    next.tag := config.genTag(header.header.pid, header.addr).asUInt
    // TODO: fix this, use an unified ID
    next.seqId := header.header.seqId.resize(16 bits)
    val isShootDown = header.header.reqType === LegoMem.RequestType.CACHE_SHOOTDOWN
    next.reqType := Mux(isShootDown, AddressLookupRequest.RequestType.SHOOTDOWN, AddressLookupRequest.RequestType.LOOKUP)
    next
  }

  def isValidAccessRequest(header : LegoMemAccessHeader) : Bool = {
    import LegoMem.RequestType._
    val req = header.header.reqType
    req === READ || req === WRITE || req === READ_MIG
  }
}

// For the interconnection, we do not need to add the names into the fifo.
// This in and outs are from Module's perspective.
// In, means outside (xbar) -> module
// Out, means module -> outside

case class LegoMemRawInterface() extends Bundle with IMasterSlave {
  val dataIn  = Stream Fragment(Bits(512 bits))
  val dataOut = Stream Fragment(Bits(512 bits))

  val ctrlIn  = Stream (ControlRequest())
  val ctrlOut = Stream (ControlRequest())

  override def asMaster(): Unit = {
    master (dataIn)
    slave (dataOut)
    master (ctrlIn)
    slave (ctrlOut)
  }

  def disableCtrl = {
    ctrlIn.freeRun()
    ctrlOut.disable
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
  io.ep.ctrlOut.translateFrom (io.raw.ctrlOut) { (axis, ctrl) =>
    axis.tdata := ctrl.asBits
    axis.tdest := ctrl.epid.resize(4)
  }

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
  // TODO: check this
  when (io.raw.dataOut.first) { io.ep.dataOut.tdata(0, LegoMemHeader.headerWidth bits) := header.asBits }


  

}
