package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4W, Axi4Aw, Axi4Config, Axi4ReadOnly, Axi4WriteOnly}
import spinal.lib.fsm.{EntryPoint, State, StateMachine}


// Input : Pte Without physical addr (ppa)
// Output : Pte With ppa
class PageFaultUint(addrWidth : Int, numPageSizes : Int) extends Component {
  val numCells = 32
  val cellWidth = log2Up(numCells)

  val pteStreamType = PageTableEntry(ppaWidth = 24, tagWidth = 48, usePteAddr = true)

  val io = new Bundle {
    val req       = slave  Stream pteStreamType
    val res       = master Stream pteStreamType
    val addrFifos = Vec(slave Stream UInt(40 bits), numPageSizes)

    val repRes = slave Flow UInt(cellWidth bits)
    val memWriteData = master Stream pteStreamType
    val memWriteAddr = master Stream UInt(cellWidth bits)
  }

  val cam = new LookupTableStoppable(48, pteStreamType.packedWidth, numCells)

  val isPagefault = !io.req.payload.valid && io.req.payload.used
  val Seq(bypass, alloc) = StreamDemux(io.req, isPagefault.asUInt, 2)

  // The page fault Path
  val pageFaultPath = new Area {
    // TODO: need a whole Lookup table here
    cam.io.wr.shootDown << io.repRes

    cam.io.rd.req << alloc.fmap(pte => {
      val cmd = cam.io.rd.req.payloadType()
      cmd.key := pte.tag
      cmd
    })

    // TODO: change this to Addr only
    val Seq(cacheAddr, allocAddr) = cam.io.rd.res.hit.demux(cam.io.rd.res.fmap(_.value.as(pteStreamType)))
    val selectedAddr = StreamMux(allocAddr.pageType, io.addrFifos.toSeq)
    val newPte = (allocAddr >*< selectedAddr).bimap ((pte, ppa) => {
      when (!pte.used) { pte.ppa := ppa }
      pte
    })
    val afterAlloc = StreamReorder(cacheAddr, allocAddr)(_.seqId)
  }

  // Report to the control agent
  // Send to the write unit
  val Seq(faultAddr, faultRpt, faultFwd) = StreamFork(pageFaultPath.afterAlloc, 3)

  cam.io.wr.req << faultRpt.fmap(pte => {
    val next = cam.io.wr.req.payloadType()
    next.value := pte.asBits.asUInt
    next.key := pte.tag
    next.mask := 0
    next.used := True
    next
  })

  io.memWriteData << faultRpt
  io.memWriteAddr << cam.io.wr.res.asStream

  // The matters here; we have reorder unit
  io.res <-< StreamReorder(bypass, faultFwd)(_.seqId)

}

class PageTableWriter(addrWidth : Int, ptePackedWidth : Int) extends Component {
  val config = Axi4Config(
    addressWidth = addrWidth,
    dataWidth = ptePackedWidth,
    useId = false,
    useRegion = false,
    useBurst = false,
    useLock = false,
    useQos = false,
    useLen = false,
    useResp = false,
    useProt = false,
    useCache = false
  )
  val io = new Bundle {
    val bus = master (Axi4WriteOnly(config))
    val reqData = slave Stream PageTableEntry(ppaWidth = 24, useTag = true, tagWidth = 48, usePteAddr = true)
    val reqAddr = slave Stream UInt(addrWidth bits)
    val res = slave Flow UInt(addrWidth bits)

  }

  val (dataIn, addrIn) = (io.reqData >*< io.reqAddr).traverse
  addrIn.toFlow >> io.res

  val (cmd, data) = StreamFork2(dataIn)

  io.bus.writeCmd << cmd.fmap(r => {
    val next = new Axi4Aw(config)
    next.addr := r.pteAddr
    next
  })

  io.bus.writeData << data.fmap(r => {
    val next = new Axi4W(config)
    next.data := r.asBits
    next
  })

  // TODO: add this feedback to agent
  io.bus.writeRsp.freeRun()

}

class FetchUnit(addrWidth : Int) extends Component {
  val numLines = 4
  val entryWidth = 64
  val axi4Config = Axi4Config(
    addressWidth = addrWidth,
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

  val outputPteType = PageTableEntry(ppaWidth = 48, tagWidth = 48, usePteAddr = true)
  val io = new Bundle {
    val bus = master (Axi4ReadOnly(axi4Config))
    val req = master Stream PageTableEntry(usePpa = false, tagWidth = 48)
    val res = master Stream outputPteType
  }

  val hashCtrl = new Area {
    val readAddr : Flow[UInt] = getAddress(almostFull.ready)

    tagFifo.io.push.payload := io.req.tag
    // TODO: check this
    tagFifo.io.push.valid := io.bus.readCmd.fire
  }

  val fifo = new StreamFifoLowLatency(io.bus.readCmd.payload, 1, 0)
  val almostFull = WaterMarkFifo(fifo.io.pop, 32)
  val readCtrl = new Area {
    // use a fifo to set ready to high
    almostFull >> io.bus.readCmd

    val readValid = hashCtrl.readAddr.valid & fifo.io.push.ready

    fifo.io.push.valid := readValid
    fifo.io.push.addr := hashCtrl.readAddr.payload
    fifo.io.push.size := Axi4.size.BYTE_64.asUInt
    fifo.io.push.len := numLines - 1
    fifo.io.push.setBurstINCR
  }

  val retFifo = StreamFifo(UInt(512 bits), 64)
  val tagFifo = StreamFifo(UInt(32 bits), 64)

  val matchCtrl = new Area {
    val numPTEperBeat = 512 / 64
    val ready = Bool

    val valid = Reg(Bool) init False
    val selected = Reg (outputPteType)

    retFifo.io.pop.ready := ready
    tagFifo.io.pop.ready := retFifo.io.pop.fire

    when (retFifo.io.pop.fire) {
      val ptes = retFifo.io.pop.payload
                               .as(Vec(Bits(128 bits), numPTEperBeat))
                               .map(cloneOf(outputPteType).fromBits(_))
      val matchVec = ptes.map(pte => pte.valid && pte.tag === tagFifo.io.pop.payload)

      selected := PriorityMux(matchVec, ptes)
      valid := matchVec.reduce(_ || _)
    }
  }

  val writeCtrl = new Area {
    val valid, validAcc = Reg(Bool) init False
    val counter = Counter(0 until numLines)


    val pte = RegNextWhen(matchCtrl.selected, matchCtrl.valid)
    // TODO: check Flow interface
    when (matchCtrl.valid) {
      counter.increment()
      validAcc := validAcc || matchCtrl.valid
    }

    when (counter.willOverflow) {
      valid := validAcc
    }

    io.res.valid := valid
    io.res.payload := pte
    when (io.res.fire) {
      valid := False
    }

  }

  // Hash function, have some delay
  def getAddress(enable: Bool) : Flow[UInt] = {
    val flow = Flow (UInt(40 bits))
    flow.payload := RegNextWhen(io.res.payload.ppa, enable)
    flow.valid := True
    flow
  }
}

object UpdateFunctions {
  // Update the PTE based on the hit infomation at cell level
  def fifoUpdate(alloc : Flow[UInt], hit : Flow[UInt], shootDown : Flow[UInt]) : Stream[UInt] = {
     alloc.asStream.queue(32)
  }
}

class UpdateUnit(
                  tagWidth : Int,
                  pteWidth : Int,
                  numCells : Int,
                  updateFunction : (Flow[UInt], Flow[UInt], Flow[UInt]) => Stream[UInt]
                ) extends Component {

  assert(isPow2(numCells))
  val cellWidth = log2Up(numCells)

  val io = new Bundle {
    // Communication FIFOs
    val hit          = slave Flow UInt(cellWidth bits)
    val shootDownReq = slave Flow UInt(cellWidth bits)

    val wrKey   = out UInt(tagWidth bits)
    val wrValue = out UInt(pteWidth bits)
    val wrAddr  = out UInt(cellWidth bits)
    val wren    = out Bool
  }

  val ids = new IDPool(numCells)
  val nextEvictionId = new Stream(UInt(cellWidth bits))

  ids.io.free << io.shootDownReq
  nextEvictionId << updateFunction(writeCtrl.nextId.tapAsFlow, io.hit, io.shootDownReq)

  val writeCtrl = new Area {
    val nextId = StreamArbiterFactory.lowerFirst.onArgs(ids.io.alloc, nextEvictionId)

  }

}

// TODO: make this a factory
// Basically control agent is a fifo arbiter. It handles two kinds of FIFOs: Sync ones and Async ones.
// SoC should make sure the FIFO is not blocked. It also handles the number of the names
class LookupControlAgent(numAsyncFifos : Int) extends Component {
  val io = new Bundle {
    val cmdIn = slave Stream ControlRequest()
    val cmdOut = slave Stream ControlRequest()

    // master control signals
    val asyncFifos = Vec(master Stream UInt(40 bits), numAsyncFifos)
    val shootDownCmd = master Flow UInt(20 bits)

    // report to master
    val hitRpt = slave Flow UInt(20 bits)
    val pageFaultRpt = slave Stream UInt(width = 40 bits)
  }

  // Write Command
  val inputs = StreamDemux(io.cmdIn, io.cmdIn.cid, numAsyncFifos + 1)
  for (i <- 0 until numAsyncFifos)
    inputs(i).fmap(_.param32) >> io.asyncFifos(i)
  inputs(numAsyncFifos).fmap(_.param32).toFlow >> io.shootDownCmd

  // Read Command
  io.cmdOut << StreamArbiterFactory.sequentialOrder.onArgs(
    io.hitRpt.asStream.fmap(u => ControlRequest(0x10, param32 = u)),
    io.pageFaultRpt.fmap(u => ControlRequest(0x11, param32 = u))
  )

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
    writeCmd.used := insertReq.valid
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

class MemoryAccessUnit extends Component {
  val io = new Bundle {
    val req = slave  Stream AccessCommand(48)
    val dataIn  = slave Stream UInt(512 bits)
    val dataOut = master Stream UInt(512 bits)

    val dataWr = master Stream UInt(512 bits)
    val dataRd = slave Stream UInt(512 bits)

    // We use a bus mod!
    val bus = master (Axi4(Axi4Config(32, 32)))
  }


  val header = io.dataIn.payload.as(LegoMemHeader(48))
  val lookback = Stream(UInt(512 bits))
  lookback.continueWhen(inputFsm.forward)

  // State machine here
  // TODO: we can split this FSM into 2 fsms
  val inputFsm = new StateMachine {
    val counter = UInt(8 bits)
    val forward = Bool

    val Seq(headerIfc, dataIfc) = StreamDemux(io.dataIn, isActive(busyWrite).asUInt, 2)
    dataIfc >> io.dataWr
    headerIfc >> lookback

    io.dataOut << StreamMux(isActive(busyRead).asUInt, Seq(lookback, io.dataRd))

    val init= new State with EntryPoint
    val busyWrite = new State
    init.whenIsActive {
      when (io.dataIn.fire) {
        switch (header.reqType) {
          is (MemoryRequestType.write) { goto (busyWrite) }
          is (MemoryRequestType.read) { goto (busyRead) }
        }
      }
    }.onExit {
      counter := getBeatsHeader(header)
    }

    busyWrite.whenIsActive {
      when (io.dataIn.fire) {
        when (counter === 1) { goto (init) }
        counter := counter - 1
      }
    }

    val busyRead = new State {
    }
  }

  def getSeqIdFromHeader(header: LegoMemHeader): UInt = {
    header.seqId
  }
  def getBeatsHeader(header: LegoMemHeader): UInt = {
    header.size
  }
}


class AddressLookupUnit extends Component {
  // input: request

  // bram cache & update unit

  // demux here : to the reorder queue.

  // fetch unit

  // pagefault unit

  // merge here
  //  StreamReorder()

}


class CoreUnit extends Component {
  val io = new Bundle {
    val ifc = master (LegoMemComponentInterface(InterfaceConfig()))
  }

  // forward all data to access unit
  val acu = new MemoryAccessUnit
  io.ifc.dataIn >> acu.io.dataIn
  io.ifc.dataOut << acu.io.dataOut
  // Sequencer
  val sequencer = new Sequencer(512, 48, 256, 64)

  //  sequencer.io.req << io.ifc.cmdIn.fmap(_.asBits.asUInt)

  // Lookup
  val lookup = new Area {
    // parameters
    val tagWidth = 48
    val addrWidth = 48
    val numCacheCells = 256

    val updateUnit = new UpdateUnit(tagWidth, addrWidth, numCacheCells, UpdateFunctions.fifoUpdate)
    val fetchUnit = new FetchUnit(addrWidth)
    val pageFaultUint = new PageFaultUint(addrWidth, 3)
  }

  // Control Agent
  acu.io.req << lookup.pageFaultUint.io.res.fmap(pte => {
    val cmd = new AccessCommand(32)
    cmd
  })
}
