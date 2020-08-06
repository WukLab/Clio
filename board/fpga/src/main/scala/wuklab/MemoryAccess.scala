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

case class BitsHeader(width : Int) extends Bundle with Header[BitsHeader] {
  require(width % 8 == 0, "Width should be byte aligned")
  val bits = Bits(width bits)
  override val packedWidth = width
  override def getSize = U"16'h8"
  override def fromWiderBits(b: Bits) = {
    bits := b(widthOf(b) downBy width)
    this
  }
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
  val dataPad = Mux(regValid, dataReg, toData.fragment(dataWidth-1 downto headerWidth))
  val outData = toData.fragment(headerWidth-1 downto 0) ## dataPad

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
        io.dataOut.last := True
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
  val selectedPad = Mux(io.dataOut.first, io.headerIn.payload.asBits, tailReg)
  val dataOut = io.dataIn.fragment(offsetWidth - 1 downto 0) ## selectedPad

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
  // TODO: add queue
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
  val readDataQueueSize = 1024
  val dmaRequestQueueSize = 256

  // Assign data for mover
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }

    // debg info
    val counters = if (config.debug) Vec(out (UInt(32 bits)), 16) else null
    val lookup_counters = if (config.debug) Vec(out (UInt(32 bits)), 11) else null

    // stat info
    val stat = new Bundle {
      val latency = Vec(out (UInt(32 bits)), 4)
    }
  }

  // TODO: create a pipe trait
  val endpoint = new RawInterfaceEndpoint
  if (config.useMigrationAccelerator) {
    val mig = new MigrationEndPointPipe
    mig.io.epIn <> io.ep
    mig.io.epOut <> endpoint.io.ep
  } else {
    endpoint.io.ep <> io.ep
  }

  val packetParser = new PacketParser(512, LegoMemAccessHeader(config.virtualAddrWidth))
  val packetBuilder = new PacketBuilder(512, LegoMemAccessHeader(config.virtualAddrWidth))
  packetParser.io.dataIn << endpoint.io.raw.dataIn
  packetBuilder.io.dataOut >> endpoint.io.raw.dataOut

  // TODO: add fifo here
  val (lookupHeader, accessHeader) = StreamFork2(packetParser.io.headerOut)

  val lookup = new AddressLookupUnit
  lookup.io.bus <> io.bus.lookup
  lookup.io.ctrl.in << endpoint.io.raw.ctrlIn
  lookup.io.ctrl.out >> endpoint.io.raw.ctrlOut
  // We insert a queue here
  lookup.io.req << lookupHeader.queue(headerQueueSize).fmap(generateLookupRequest)

  val access = new MemoryAccessUnit
  lookup.io.res <> access.io.lookupRes
  accessHeader.takeBy(isValidAccessRequest).queue(headerQueueSize) >> access.io.headerIn
  packetBuilder.io.headerIn << access.io.headerOut.queue(headerQueueSize)

  val dma = new axi_dma(config.dmaAxisConfig, config.accessAxi4Config, config.physicalAddrWidth, config.dmaLengthWidth)
  dma.io.m_axi <> io.bus.access
  val (writeCmd, writeSizeCmdF) = StreamFork2(access.io.wrCmd.queue(dmaRequestQueueSize))
  val sizeCmd = writeSizeCmdF.fmap { cmd =>
    val len = cmd.len.resize(6)
    val lenTop = !len.orR
    val lenFull = (lenTop ## len).asUInt
    ((U"1'h1" << lenFull) - 1).resize(64)
  } .queue(32)
  dma.io.s_axis_read_desc << access.io.rdCmd.queue(dmaRequestQueueSize)
  dma.io.s_axis_write_desc << writeCmd
  dma.io.m_axis_read_data.liftStream(_.tdata).queue(readDataQueueSize) >> packetBuilder.io.dataIn
  // TODO: add a loopback path here, for extended module parameters
  // signal A -> loopback queue -> signal B -> access unit
  val filteredDataF = packetParser.io.dataOut.queue(writeDataQueueSize)
  val filteredData = filteredDataF.filterBySignal(access.io.wrDataSignal.queue(dmaRequestQueueSize))
  dma.io.s_axis_write_data.translateFrom (filteredData.queue(dmaRequestQueueSize)) { (axis, data) =>
    axis.fragment.tdata := data.fragment
    axis.fragment.tkeep := Mux(data.last, sizeCmd.payload.asBits, B"64'hFFFF_FFFF_FFFF_FFFF")
    axis.last := data.last
  }
  sizeCmd.ready := filteredData.lastFire

  dma.io.write_enable := True
  dma.io.read_enable := True
  dma.io.write_abort := False

  // === Stat
  val counter = Counter(32 bits, True)
  val stat = new Area {
    val dataInFirst = RegNextWhen(counter.value, packetParser.io.dataIn.firstFire)
    val dataInLast = RegNextWhen(counter.value, packetParser.io.dataIn.lastFire)
    val lookupOut = RegNextWhen(counter.value, lookup.io.res.fire)

    val accFirstLookup = Reg (UInt(32 bits)) init 0
    val accLastLookup = Reg (UInt(32 bits)) init 0
    val accFirstAccess = Reg (UInt(32 bits)) init 0

    // last to last
    when (lookup.io.res.fire) {
      accFirstLookup := accFirstLookup + (counter.value - dataInFirst)
      accLastLookup := accLastLookup + (counter.value - dataInLast)
    }

    when (packetBuilder.io.dataOut.lastFire) {
      accFirstAccess := accFirstAccess + (counter.value - lookupOut)
    }

    io.stat.latency(0) := accFirstLookup
    io.stat.latency(1) := accLastLookup
    io.stat.latency(2) := accFirstAccess
    io.stat.latency(3) := 0x7edc1234
  }


  // === Debug
  if (config.debug) {
    // Monitor registers
    val Registers = Seq[(String, Bool)](
      ("ParserDataIn", packetParser.io.dataIn.lastFire),
      ("ParserHeaderOut", packetParser.io.headerOut.fire),
      ("ParserDataOut", packetParser.io.dataOut.lastFire),
      ("BuilderDataIn", packetBuilder.io.dataIn.lastFire),
      ("BuilderHeaderIn", packetBuilder.io.headerIn.fire),
      ("BuilderHeaderOut", packetBuilder.io.dataOut.lastFire),
      ("LookupHeader", lookup.io.req.fire),
      ("AccessHeader", access.io.headerIn.fire),
      ("LookupRes", lookup.io.res.fire),

      ("writeDesc", dma.io.s_axis_write_desc.fire),
      ("readDesc", dma.io.s_axis_read_desc.fire),
      ("filterSignal", access.io.wrDataSignal.fire),
      ("FilteredSignal", filteredData.lastFire),
      ("dmaWriteData", dma.io.s_axis_write_data.lastFire),
      ("dmaReadData", dma.io.m_axis_read_data.lastFire),
      ("ctrlOUt", io.ep.ctrlOut.fire)
    )

    val regs = Registers.map(_._2).map(Counter(32 bits, _))
    (io.counters, regs).zipped map (_ := _.value)

    val base = BigInt("A0006400", 16)
    println(f"* Build Register Access Script on $base")
    Registers.map(_._1).zipWithIndex.map { case (str, idx) =>
      val addr = base + idx * 4
      println(f"echo $str")
      println(f"devmem 0x$addr%X 32")
    }

    lookup.io.counters <> io.lookup_counters
  }

  def generateLookupRequest(header : LegoMemAccessHeader) : AddressLookupRequest = {
    val next = AddressLookupRequest(config.tagWidth)
    // Currently we only fetch the smallest page sizes
    next.tag := config.genTag(header.header.pid, header.addr).asUInt
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

case class LegoMemRawDataInterface() extends Bundle with IMasterSlave {
  val dataIn = Stream Fragment (Bits(512 bits))
  val dataOut = Stream Fragment (Bits(512 bits))
  override def asMaster(): Unit = {
    master(dataIn)
    slave(dataOut)
  }
}

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

class RawInterfaceEndpoint(implicit config : LegoMemConfig) extends Component {
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
