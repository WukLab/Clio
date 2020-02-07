package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Aw, Axi4Config, Axi4ReadOnly, Axi4W, Axi4WriteOnly, Axi4WriteOnlyUpsizer}
import spinal.lib.fsm.{EntryPoint, State, StateMachine}


// Input : Pte Without physical addr (ppa)
// Output : Pte With ppa
// (ppaWidth : Int, tagWidth : Int, numPageSizes : Int)

class PageFaultUint(implicit config : CoreMemConfig) extends Component {
  // TODO: init unused registers

  val io = new Bundle {
    val req       = slave  Stream config.pteWithAddrType
    val res       = master Stream config.pteResType
    val rpt       = master Flow config.pteWithAddrType
    val addrFifos = Vec(slave Stream UInt(config.ppaWidth bits), config.numPageSizes)

    val memWriteRes = slave Flow UInt(config.pageFaultCellWidth bits)
    val memWriteData = master Stream config.pteWithAddrType
    val memWriteUser = master Stream UInt(config.pageFaultCellWidth bits)
  }

  val cam = new LookupTableStoppable(config.tagWidth, config.pteCompactWidth, config.numPageFaultCacheCells)

  // The page fault Path
  val pageFaultPath = new Area {
    cam.io.wr.shootDown << io.memWriteRes

    cam.io.rd.req.translateFrom (io.req) ((cmd, pte) => cmd.key := pte.tag)

    // delayed pipeline interface
    val originalPte = cam.delay(io.req.payload)
    val lookupResult = cam.io.rd.res.fmap(r => {
      val sel = r.hit && originalPte.used
      Mux(sel, PageTableEntry.compact(originalPte, r.value.asBits), originalPte)
    })

    val selectedAddr = StreamMux(lookupResult.pageType, io.addrFifos.toSeq)
    // merge when
    val afterAlloc = Stream(Pair(Bool, lookupResult.payloadType()))
    val mergeCtrl = new Area {
      val noAlloc = lookupResult.used || !lookupResult.allocated
      lookupResult.ready := Mux(noAlloc, afterAlloc.ready, afterAlloc.ready && selectedAddr.valid)
      selectedAddr.ready := !noAlloc && afterAlloc.ready && lookupResult.valid
      afterAlloc.valid := Mux(noAlloc, lookupResult.valid, lookupResult.valid && selectedAddr.valid)
      afterAlloc.payload.fst := noAlloc
      afterAlloc.payload.snd := lookupResult.payload
      when (!noAlloc) {
        afterAlloc.snd.used := True
        afterAlloc.snd.ppa := selectedAddr.payload
      }
    }
  }

  // Report to the control agent
  // Send to the write unit
  val Seq(bramWr, faultRpt, faultFwd) = StreamFork(pageFaultPath.afterAlloc.stage, 3)

  // TODO: check this write path, add new content
  cam.io.wr.req.translateFrom (bramWr.throwWhen(bramWr.fst)) ((next, pte) => {
    next.value := pte.snd.asCompactBits.asUInt
    next.key := pte.snd.tag
    next.mask := 0
  })

  val pageFaults = faultRpt.throwWhen(faultRpt.fst).fmap(_.snd)
  io.memWriteData << pageFaults
  io.memWriteUser << cam.io.wr.res.asStream

  io.rpt << pageFaults.tapAsFlow

  io.res << faultFwd.fmap(_.snd)
}

// (ppaWidth : Int, tagWidth : Int, busAddrWidth : Int, userWidth : Int)
class PageTableWriter(implicit config: CoreMemConfig) extends Component {
  val memoryLatency = 32
  val memConfig = Axi4Config(
    addressWidth = config.hashtableAddrWidth,
    dataWidth = config.pteWithAddrType.packedWidth,
    useId = false,
    useRegion = false,
    useBurst = false,
    useLock = false,
    useQos = false,
    useLen = false,
    useResp = true,
    useProt = false,
    useCache = false,
    useLast = false,
    useStrb = false
  )

  val io = new Bundle {
    val bus = master (Axi4WriteOnly(memConfig))
    val reqData = slave Stream config.pteWithAddrType
    val reqUser = slave Stream UInt(config.pageFaultCellWidth bits)
    val res = master Flow UInt(config.pageFaultCellWidth bits)
  }

  val userFifo = StreamFifo(UInt(config.pageFaultCellWidth bits), memoryLatency)

  io.reqUser >> userFifo.io.push

  val (cmd, data) = StreamFork2(io.reqData)
  io.bus.writeCmd.translateFrom  (cmd)  ((cmd, pte) => {
    cmd.addr := config.getPteBusAddr(pte.pteAddr)
    cmd.size := Axi4.size.BYTE_16.asUInt
  })
  io.bus.writeData.translateFrom (data) ((cmd, pte) => cmd.data := pte.asBits)

  io.res << (io.bus.writeRsp.stage() *> userFifo.io.pop).toFlow
}

trait HashFunction extends StoppablePipeline {
  val outputWidth : Int
  def hash(input : UInt) : UInt
}
class LinearHashFunction(val outputWidth: Int, enable : Bool) extends HashFunction {
  override def pipelineDelay = 1

  override def hash(input: UInt) : UInt = {
    RegNextWhen(input, enable)(outputWidth-1 downto 0)
  }

  override def enableSignal = enable

}

class FetchUnit(implicit config: CoreMemConfig) extends Component {

  val hashTableBaseAddr = U(0, config.hashtableAddrWidth bits)

  val axi4Config = Axi4Config(
    addressWidth = config.hashtableAddrWidth,
    dataWidth = 512,
    useId = false,
    useRegion = false,
    useBurst = true,
    useLock = false,
    useQos = false,
    useLen = true,
    useResp = false,
    useProt = false,
    useCache = false
  )

  val io = new Bundle {
    val bus = master (Axi4ReadOnly(axi4Config))
    val req = slave Stream config.lookupReqType
    val res = master Stream config.pteWithAddrType
  }

  val fifo = new StreamFifoLowLatency(io.bus.readCmd.payloadType, 1, 0)
  val almostFull = WaterMarkFifo(fifo.io.pop, 32)

  // These fifos are part of the watermark system
  val retFifo = StreamFifo(Bits(512 bits), 64)
  val reqFifo = StreamFifo(new Bundle {
    val tag = cloneOf(io.req.tag)
    val seqId = cloneOf(io.req.seqId)
    val addr = UInt(config.pteAddrWidth bits)
  }, 64)

  val hashCtrl = new Area {
    io.req.ready := almostFull.ready

    val hashFunc = new LinearHashFunction(config.pteAddrWidth, almostFull.ready)
    val readAddr = new Flow(UInt(config.pteAddrWidth bits))
    readAddr.payload := hashFunc.hash(io.req.tag)
    readAddr.valid := hashFunc.delay(io.req.fire) init False

    val delayedReq = hashFunc.delayFlow(io.req.payload, io.req.fire)
    reqFifo.io.push.tag := delayedReq.tag
    reqFifo.io.push.addr := readAddr.payload
    reqFifo.io.push.seqId := delayedReq.seqId
    reqFifo.io.push.valid := io.bus.readCmd.fire
  }

  val readCtrl = new Area {
    // use a fifo to set ready to high
    almostFull >> io.bus.readCmd

    val readValid = hashCtrl.readAddr.valid & fifo.io.push.ready

    fifo.io.push.valid := readValid
    fifo.io.push.addr := config.getPteBusAddr(hashCtrl.readAddr.payload)
    fifo.io.push.size := Axi4.size.BYTE_64.asUInt
    fifo.io.push.len := config.linePerBucket - 1
    fifo.io.push.setBurstINCR
  }

  val matchCtrl = new Area {
    // TODO : find back pressure signal
    val ready = True

    val valid = RegNext(retFifo.io.pop.fire) init False
    val hit = Reg(Bool) init False
    val selected = Reg (config.pteWithAddrType)

    io.bus.r.fmap(_.data) >> retFifo.io.push

    retFifo.io.pop.ready := ready
    reqFifo.io.pop.ready := retFifo.io.pop.fire

    when (retFifo.io.pop.fire) {
      val ptes = retFifo.io.pop.payload
        .as(Vec(Bits(config.pteFullWidth bits), config.ptePerLine))
        .map(cloneOf(config.pteWithAddrType).fromBits(_))
      // TODO: Change tag fifo to this
      for ((pte, i) <- ptes.zipWithIndex) {
        pte.seqId := reqFifo.io.pop.seqId
        pte.pteAddr := reqFifo.io.pop.addr + (U(i, config.pteAddrWidth bits) |<< log2Up(config.pteFullBytes))
      }
      val matchVec = ptes.map(pte => pte.allocated && pte.tag === reqFifo.io.pop.tag)

      selected := PriorityMux(matchVec, ptes)
      hit := matchVec.reduce(_ || _)
    }
  }

  val writeCtrl = new Area {
    val counter = Counter(0 until config.linePerBucket, matchCtrl.valid)
    val pte = RegNextWhen(matchCtrl.selected, matchCtrl.hit && matchCtrl.valid)

    when (counter.willOverflow) {
      pte.allocated.clear()
      pte.used.clear()
    }

    io.res.valid := counter.willOverflow
    io.res.payload := pte
  }

}

object UpdateFunctions {
  // Update the PTE based on the hit infomation at cell level
  def fifoUpdate(alloc : Flow[UInt], hit : Flow[UInt], shootDown : Flow[UInt]) : Stream[UInt] = {
     alloc.asStream.queue(32)
  }
}

class UpdateUnit(
                  updateFunction : (Flow[UInt], Flow[UInt], Flow[UInt]) => Stream[UInt]
                )(implicit config : CoreMemConfig) extends Component {

  val io = new Bundle {
    // Communication FIFOs
    val rpt = new Bundle {
      val hit = slave Flow UInt(config.cacheCellWidth bits)
      val shootDown = slave Flow UInt(config.cacheCellWidth bits)
      val alloc = slave Flow UInt(config.cacheCellWidth bits)
      val update = slave Flow config.pteWithPpaType
    }

    val insertReq = master Stream LookupWrite(config.tagWidth, rpt.update.compactWidth, useUsed = true, usedWidth = config.cacheCellWidth)
  }

  // This flow is always ready
  val nextEvictionId = new Stream(UInt(config.cacheCellWidth bits))
  nextEvictionId << updateFunction(io.rpt.alloc, io.rpt.hit, io.rpt.shootDown)

  io.insertReq.translateFrom (io.rpt.update.asStream >*< nextEvictionId)((cmd, p) => {
    val Pair(pte, id) = p
    cmd.usedCell := id
    cmd.key := pte.tag
    cmd.mask := config.getMask(pte.pageType)
    cmd.value := pte.asCompactBits.asUInt
  })

}

// Wrapper of the cache, change io to the request, add mux and demux
class BramCache(implicit config : CoreMemConfig) extends Component {

  val table = new LookupTableStoppable(config.tagWidth, config.pteCompactWidth, config.numCacheCells,
    useUsed = true, autoUpdate = true, useHitReport = true)

  val io = new Bundle {
    val cmd = new Bundle {
      val req = slave Stream config.lookupReqType
      val fwd = master Stream config.lookupReqType
      val res = master Stream config.pteWithPpaType
    }

    val ctrl = new Bundle {
      val insertReq = slave Stream table.io.wr.req.payloadType()
      val hitRpt = master Flow UInt(config.cacheCellWidth bits)
      val allocRpt = master Flow UInt(config.cacheCellWidth bits)
      val shootDownRpt = master Flow UInt(config.cacheCellWidth bits)
    }

  }

  io.ctrl.insertReq >> table.io.wr.req
  io.ctrl.allocRpt << table.io.wr.res

  val lookupCtrl = new Area {
    io.cmd.req.fmap(r => LookupReadReq(r.tag)) >> table.io.rd.req
    // read out
    // TODO: fix this constant for shoot down
    val delayedCmd = table <|| io.cmd.req
    val isShootDown = delayedCmd.reqType === 1
    val isResult = table.io.rd.res.hit

    val Seq(fwd, res) = isResult.demux(table.io.rd.res.throwWhen(isShootDown))
    // forward path
    io.cmd.fwd <-< (delayedCmd <* fwd)
    // result path
    // TODO: change this assignment
    val pte = io.cmd.res.payloadType()
    pte.seqId := delayedCmd.seqId
    io.cmd.res << res.fmap(r => pte.fromCompactBits(r.value.asBits))

    val Seq(hitRpt, shootDownRpt) = isShootDown.demux(table.io.rd.rpt)
    io.ctrl.hitRpt << hitRpt
    io.ctrl.shootDownRpt << shootDownRpt
    table.io.wr.shootDown << shootDownRpt
  }



}

// TODO: make this a factory
// Basically control agent is a fifo arbiter. It handles two kinds of FIFOs: Sync ones and Async ones.
// SoC should make sure the FIFO is not blocked. It also handles the number of the names
class LookupControlAgent(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    val cmdIn = slave Stream ControlRequest()
    val cmdOut = master Stream ControlRequest()

    // master control signals
    val asyncFifos = Vec(master Stream UInt(config.ppaWidth bits), config.numPageSizes)

    // report to master
    val hitRpt = slave Flow UInt(config.cacheCellWidth bits)
    val pageFaultRpt = slave Flow UInt(config.pteAddrWidth bits)
  }

  // TODO: add fifo length here

  // Write Command
  val numOutputFifos = config.numPageSizes
  val inputs = StreamDemux(io.cmdIn, io.cmdIn.cid(log2Up(numOutputFifos)-1 downto 0), numOutputFifos)
  for (i <- 0 until numOutputFifos)
    inputs(i).fmap(resizeParam(_, config.ppaWidth)) >> io.asyncFifos(i)

  // Read Command
  io.cmdOut << StreamArbiterFactory.onArgs(
    io.hitRpt.asStream.fmap(u => ControlRequest(0x10, param32 = u.resize(32))),
    io.pageFaultRpt.asStream.fmap(u => ControlRequest(0x11, param32 = u.resize(32)))
  )

  def resizeParam(req : ControlRequest, size : Int): UInt = {
    assert(size <= 40, "over sized prameter")
    (req.param8 ## req.param32).resize(size).asUInt
  }

}

// Return the state change from zero
abstract class CounterSet(dataWidth : Int, numCells : Int) extends Area {
  def inc(addr : UInt) : Bool
  def dec(addr : UInt) : Bool
}

class RegisterCounterSet(dataWidth : Int, numCells : Int) extends CounterSet(dataWidth, numCells) {
  val vec = Vec(Reg (UInt(dataWidth bits)) init 0, numCells)
  override def inc(addr: UInt): Bool = {
    val change = vec(addr) === 0
    vec(addr) := vec(addr) + 1
    change
  }
  override def dec(addr: UInt): Bool = {
    val change = vec(addr) === 1
    vec(addr) := vec(addr) - 1
    change
  }
}

// TODO: add more implementations, like Vec(Counter)
// Here we should use a register implementation first
class LockCounterRam(numWaits : Int, numCells : Int) extends Component {

  assert(isPow2(numCells))
  val cellWidth = log2Up(numCells)
  val waitWidth = log2Up(numWaits)

  val io = new Bundle {
    val lockReq   = slave Flow UInt(cellWidth bits)
    val unlockReq = slave Flow UInt(cellWidth bits)

    // The freed address
    val freeAddr  = master Flow UInt(cellWidth bits)
    val isLocked  = out Vec(Bool, numCells)

    val popReq    = slave Flow UInt(cellWidth bits)
  }

  val lockCounts = new RegisterCounterSet(waitWidth, numCells)
  val waitCounts = new RegisterCounterSet(waitWidth, numCells)

  object cellState extends SpinalEnum {
    val idle, locked, clearing = newElement()
  }

  val lockedReg = Vec (Reg (cellState()) init cellState.idle, numCells)
  io.isLocked := Vec (lockedReg.map(_ === cellState.locked))

  // LockteCtrl
  when (io.lockReq.valid) {
    waitCounts.inc(io.lockReq.payload)
  }

  val conflict = io.lockReq.valid && io.unlockReq.valid && (io.lockReq.payload === io.unlockReq.payload)
  when (!conflict) {
    when(io.lockReq.valid) {
      when(lockCounts.inc(io.lockReq.payload)) {
        lockedReg(io.lockReq.payload) := cellState.locked
      }
    }

    when (io.unlockReq.valid) {
      when(lockCounts.dec(io.unlockReq.payload)) {
        lockedReg(io.unlockReq.payload) := cellState.clearing
      }
    }
  }

  // Pop Ctrl
  io.freeAddr.valid := False
  io.freeAddr.payload := io.popReq.payload
  when (io.popReq.fire) {
    val isEmpty = waitCounts.dec(io.popReq.payload)
    when (isEmpty) {
      io.freeAddr.valid := True
      lockedReg(io.popReq.payload) := cellState.idle
    }
  }

}

class Sequencer(dataWidth : Int, tagWidth : Int, numWaits : Int, numCells : Int) extends Component {

  assert(isPow2(numCells))
  val cellWidth = log2Up(numCells)

  val io = new Bundle {
    val req = slave  Stream InternalMemoryRequest(dataWidth)
    val res = master Stream InternalMemoryRequest(dataWidth)

    val unlock = slave Flow UInt(cellWidth bits)
  }

  // Write : Match -> If exists, add; else, insert
  val cam = new LookupTCamStoppable(dataWidth, cellWidth, true)
  val ids = new IDPool(numCells)
  val lock = new LockCounterRam(numWaits, numCells)

  // Internal infomations
  val bypassFifo = new StreamFifoLowLatency(io.req.payloadType, 1)

  // Data path
  val inputCtrl = new Area {

    // flow through CAM
    val afterLookup = requireLock(io.req)

    // See if we need lock
    val newLock = isLockInstruction(afterLookup.snd)
    val requireLockBool =  afterLookup.fst.hit || newLock

    // split to two flows
    val beforeWait = afterLookup.fmapFst(_.value)
    val Seq(bypassPort, waitPort) = StreamDemux(beforeWait, requireLockBool.asUInt, 2)
    bypassPort.fmap(_.snd) >> bypassFifo.io.push
  }

  val waitFifo   = new StreamFifo(inputCtrl.beforeWait.payloadType, numWaits)

  val lockAddrCtrl = new Area {
    // TODO: this is arrow.
    val Seq(existingLock, newLock) = inputCtrl.newLock.demux(inputCtrl.waitPort)

    // Bind this two fifos
    val newCmd = ids.io.alloc >*< newLock.fmap(_.snd)
    val nextCmd = StreamMux(inputCtrl.newLock.asUInt, Seq(existingLock, newCmd))
//    val nextCmd =  inputCtrl.newLock.mux(existingLock, newCmd)

    nextCmd >> waitFifo.io.push
    nextCmd.tapAsFlow.fmap(_.fst) >> lock.io.lockReq
  }

  val camCtrl = new Area {
    val insertReq = lockAddrCtrl.newCmd.tapAsFlow
    val deleteReq = lock.io.freeAddr

    val writeCmd = cloneOf (cam.io.wr.payload)

    // Set the command
    writeCmd.key := getTagFromRequest(insertReq.snd)
    writeCmd.mask := getMaskFromRequest(insertReq.snd)
    // If is not insert, then used is false (it is a delete)
    writeCmd.enable := insertReq.valid
    writeCmd.value := Mux(deleteReq.valid && insertReq.valid, deleteReq.payload, insertReq.payload.fst)

    cam.io.wr << ReturnFlow(writeCmd, insertReq.valid || deleteReq.valid)

    // Return of address
    ids.io.free <-< deleteReq.throwWhen(insertReq.valid)
  }

  val outputCtrl = new Area {
    // unlock forward
    io.unlock >> lock.io.unlockReq
    waitFifo.io.pop.tapAsFlow.fmap(_.fst) >> lock.io.popReq

    // output path
    val unlockLast = ~lock.io.isLocked(waitFifo.io.pop.fst)
    io.res << StreamArbiterFactory.onArgs(
      bypassFifo.io.pop,
      waitFifo.io.pop.fmap(_.snd).continueWhen(unlockLast)
    )
  }

  def getTagFromRequest (req: InternalMemoryRequest) : UInt = {
    req.addr
  }

  def getMaskFromRequest (req: InternalMemoryRequest) : UInt = {
    req.mask
  }

  def requireLock (req : Stream[InternalMemoryRequest]) = {
    cam.io.rd.req << io.req.fmap(_ |> getTagFromRequest |> LookupReadReq.apply)

    val cmd = cam.delay(req.payload)
    cam.io.rd.res fmap (Pair(_, cmd))
  }

  def isLockInstruction (req : InternalMemoryRequest) : Bool = {
    req.reqType === MemoryRequestType.alloc || req.reqType === MemoryRequestType.free
  }

}

class MemoryAccessUnit(phyAddrWidth : Int) extends Component {
  val lookbackSize = 256

  val io = new Bundle {
    val req = slave  Stream PageTableEntry(useTag = false, ppaWidth = 24)
    val dataIn  = slave Stream UInt(512 bits)
    val dataOut = master Stream UInt(512 bits)

    val wrCmd = master Stream DataMoverCmd(phyAddrWidth, true)
    val rdCmd = master Stream DataMoverCmd(phyAddrWidth, true)
    val dataWr = master Stream UInt(512 bits)
    val dataRd = slave Stream UInt(512 bits)

    // We use a bus mod!
    val bus = master (Axi4(Axi4Config(32, 32)))
  }


  val header = io.dataIn.payload.as(LegoMemHeader(48))
  val lookback = StreamFifo(UInt(512 bits), lookbackSize)

  // State machine here
  // TODO: we can split this FSM into 2 fsms, this one push things into the loopback
  // This one send out commands, parse heads, forward things
  val inputFsm = new StateMachine {
    val init= new State with EntryPoint
    val busyWrite = new State

    val counter = UInt(8 bits)

    val Seq(headerIfc, dataIfc) = isActive(busyWrite).demux(io.dataIn)
    dataIfc >> io.dataWr
    // Generate header
    headerIfc >> lookback.io.push
    // repl



    // a.stage.translateInto(checked instruction)
    // OR, in -> a, f(a), a -> b

    // Commands can be generated outside of the state machine. just need
    init.whenIsActive {
      when (io.dataIn.fire) {
        switch (header.reqType) {
          is (MemoryRequestType.write) {
            goto (busyWrite)
            // generate commands
          }
        }
      }
    }

    busyWrite.whenIsActive {
      when (io.dataIn.fire) {
        when (counter === 1) { goto (init) }
        counter := counter - 1
      }
    }
  }

  // This FSM pull things out of loopback and dataFifo. This is simple!
  val outputCtrl = new StateMachine {
    val init= new State with EntryPoint
    val busyRead = new State
    io.dataOut << isActive(busyRead).select(lookback.io.pop, io.dataRd)

  }

  def getSeqIdFromHeader(header: LegoMemHeader): UInt = {
    header.seqId
  }
  def getBeatsHeader(header: LegoMemHeader): UInt = {
    header.size
  }

//  def commandCheck() : Bool = {
//
//  }
}


class AddressLookupUnit(implicit config : CoreMemConfig) extends Component {
  val axi4Config = Axi4Config(
    addressWidth = config.hashtableAddrWidth,
    dataWidth = 512,
    useId = false,
    useRegion = false,
    useBurst = true,
    useLock = false,
    useQos = false,
    useLen = true,
    useResp = false,
    useProt = false,
    useCache = false,
    useStrb = false
  )

  // input: request
  val io = new Bundle {
    val req = slave Stream config.lookupReqType
    val res = master Stream config.pteResType
    val ctrl = new Bundle {
      val in = slave Stream ControlRequest()
      val out = master Stream ControlRequest()
    }
    val bus = master (new Axi4(axi4Config))
  }

  // TODO: rething this arrows. if theres timing violation, first add pipelines here
  // Control Agent
  val agent = new LookupControlAgent
  io.ctrl.in >> agent.io.cmdIn
  io.ctrl.out << agent.io.cmdOut

  // bram cache & update unit
  val cache = new BramCache
  val update = new UpdateUnit(UpdateFunctions.fifoUpdate)
  cache.io.cmd.req << io.req
  cache.io.ctrl.insertReq << update.io.insertReq
  update.io.rpt.hit << cache.io.ctrl.hitRpt
  update.io.rpt.alloc << cache.io.ctrl.allocRpt
  update.io.rpt.shootDown << cache.io.ctrl.shootDownRpt

  cache.io.ctrl.hitRpt >> agent.io.hitRpt

  // fetch unit
  val fetcher = new FetchUnit
  fetcher.io.req << cache.io.cmd.fwd

  // pagefault unit
  val pageFault = new PageFaultUint
  val writer = new PageTableWriter
  pageFault.io.req << fetcher.io.res
  pageFault.io.memWriteRes << writer.io.res
  (pageFault.io.addrFifos, agent.io.asyncFifos).zipped map (_ << _)
  writer.io.reqData << pageFault.io.memWriteData
  writer.io.reqUser << pageFault.io.memWriteUser

  pageFault.io.rpt >> update.io.rpt.update
  pageFault.io.rpt.fmap(_.pteAddr) >> agent.io.pageFaultRpt

  // bus assignment
  val writeAdapter = Axi4WriteOnlyNonBurstUpsizer(writer.io.bus.config, io.bus.config)
  writer.io.bus >> writeAdapter.io.input
  writeAdapter.io.output >> io.bus
  fetcher.io.bus >> io.bus

  // merge here
  // TODO: make this a component
  val counter = Counter(16 bits, io.res.fire)
  val streams = Seq(cache.io.cmd.res, pageFault.io.res)
  io.res << StreamMux(OHToUInt(streams.map(_.seqId === counter.value)), streams)

}

