package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.core.internals.Operator
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4
import spinal.lib.bus.amba4.axi.{Axi4Config, Axi4ReadOnly}
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Config, AxiLite4SlaveFactory}
import spinal.lib.fsm.{EntryPoint, State, StateMachine}

//Hardware definition
class MyTopLevel extends Component {
  val axiliteConfig = new AxiLite4Config(32, 32)

  val io = new Bundle {
    val configBus = slave (AxiLite4(axiliteConfig))
    val cond0 = in  Bool
    val cond1 = in  Bool
    val flag  = out Bool
    val state = out UInt(8 bits)
  }
  val counter = Reg(UInt(8 bits)) init(0)

  when(io.cond0) {
    counter := counter + 1
  }

  val regCtrl = new AxiLite4SlaveFactory(io.configBus)
  val reg = Reg(UInt(32 bits)) init 0xdead
  regCtrl.readAndWrite(reg, 0)

  io.state := counter
  io.flag  := (counter === 0) | io.cond1
}

class PageFaultUint(addrWidth : Int, numPageSizes : Int) extends Component {
  val cellWidth = 16

  val io = new Bundle {
    val req       = slave  Stream PTE(addrWidth)
    val res       = master Stream PTE(addrWidth)
    val addrFifos = Vec(slave Stream UInt(40 bits), numPageSizes)

    val rep   = master Flow PTE(addrWidth)
  }

  val isPagefault = !io.req.payload.valid && io.req.payload.used
  val Seq(bypass, alloc) = StreamDemux(io.req, isPagefault.asUInt, 2)

  // The page fault Path
  val pageFaultPath = new Area {
    // TODO: trait type lookup
    val cam = new TernaryCAM(addrWidth, cellWidth)
    val valid = Delay(alloc.valid, cam.readDelay)

    val afterLookup = alloc.fmap(_.withValid(cam.io.rdvalid)).continueWhenPipeline(valid, 2)
    val beforeAlloc = afterLookup.haltWhen(io.addrFifos.map(_.ready).reduce(_ && _))
    val afterAlloc = (beforeAlloc fmap fetchFromAsyncFifos).stage()
  }

  // Report to the control agent
  io.rep <-< pageFaultPath.afterAlloc.asFlow

  // The matters here; we have reorder unit
  io.res <-< StreamReorder(bypass, pageFaultPath.afterAlloc)(_.tag)

  def fetchFromAsyncFifos (pte : Valid[PTE]) : PTE = {
    val res = pte.payload

    io.addrFifos.foreach(_.ready.clear())
    when (~pte.valid) {
      pte.ppa := io.addrFifos(pte.pageType).payload
      io.addrFifos(pte.pageType).ready := True
    }

    res
  }
}

class FetchUnit(addrWidth : Int) extends Component {
  val numLines = 4
  val axi4Config = Axi4Config(
    addressWidth = addrWidth,
    dataWidth = 32,
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
    val req = master Stream UInt(40 bits)
    val res = master Stream PTE(addrWidth)
  }


  val hashCtrl = new Area {
    val readAddr : Flow[UInt] = getAddress(almostFull.ready)

    tagFifo.io.push.payload := io.req.payload
    // TODO: check this
    tagFifo.io.push.valid := io.bus.readCmd.fire
  }

  val fifo = new StreamFifoLowLatency(io.bus.readCmd.payload, 1, 0)
  val almostFull = WaterMarkFifo(fifo.io.pop, 32)
  val readCtrl = new Area {
    // use a fifo to set ready to high
    // TODO: replace this with a registerFIFO
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
    val selected = Reg (PTE(48 )

    retFifo.io.pop.ready := ready
    tagFifo.io.pop.ready := retFifo.io.pop.fire

    when (retFifo.io.pop.fire) {
      val ptes = retFifo.io.pop.payload.as(Vec(UInt(64 bits), numPTEperBeat)).map(_.as(PTE(addrWidth)))
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

// Two step write:

class PTECache(tagWidth : Int, pteWidth : Int, numCells : Int) extends Component {

  assert(isPow2(numCells))
  val cellWidth = log2Up(numCells)

  val io = new Bundle {
    val wrKey   = in UInt(tagWidth bits)
    val wrValue = in UInt(pteWidth bits)
    val wrAddr = in UInt(cellWidth bits)
    val wren = in Bool

    val req = slave  Stream LookupResult()
    val res = master Stream LookupResult()
  }

  val tcam = new TernaryCAM(tagWidth, cellWidth)
  val bram = new Mem(UInt(pteWidth bits), numCells)

  // Write will stall the read path, Since we need the read
  val writeCtrl = new Area {
    // Write CAM in the first cycle
    tcam.io.wren := io.wren
    tcam.io.wrkey := io.wrKey
    tcam.io.wraddr := io.wrAddr

    // Write Register in the next cycle
    when (RegNext(io.wren)) {
      bram(RegNext(io.wrAddr)) := RegNext(io.wrValue)
    }
  }

  val readCtrl = new Area {
    io.req.ready := ~io.wren
    tcam.io.rdvalid := io.req.valid
    tcam.io.rdkey := io.req.pa

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
    val dramUpdate   = slave Stream PTELookupResult(true, true)
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

// Basically control agent is a fifo arbiter. It handles two kinds of FIFOs: Sync ones and Async ones.
// SoC should make sure the FIFO is not blocked. It also handles the number of the names
class LookupControlAgent(numAsyncFifos : Int) extends Component {
  val io = new Bundle {
    val cmdIn = slave Stream ControlRequest()
    val cmdOut = slave Stream ControlRequest()

    // master control signals
    val asyncFifos = Vec(master Stream UInt(40 bits), numAsyncFifos)
    val shootDownCmd = master Flow UInt(20 bits)

    // master to slave
    val hitRpt = slave Flow UInt(20 bits)
    val pageFaultRpt = slave Stream
  }

  // Write Command
  switch (io.cmdOut.cmd) {
    is (0) {

    }

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

    val freeAddr  = slave Flow UInt(cellWidth bits)
    val isLocked  = out Vec(Bool, numCells)

    val popReq    = slave Flow UInt(cellWidth bits)
  }

  val lockCounts = new RegisterCounterSet(waitWidth, numCells)
  val waitCounts = new RegisterCounterSet(waitWidth, numCells)

  object cellState extends SpinalEnum {
    val idle, locked, clearing = newElement()
  }

  val lockedReg = Vec (Reg (cellState()) init cellState.idle, numCells)
  io.isLocked := Vec (lockedReg.map(_ =/= cellState.idle))

  // LockteCtrl
  val conflict = io.lockReq.valid && io.unlockReq.valid && (io.lockReq.payload === io.unlockReq.payload)
  when (!conflict) {
    when(io.lockReq.valid) {
      // TODO: another imlementation: copy when unlock
      waitCounts.inc(io.lockReq.payload)
      when(lockCounts.inc(io.lockReq.payload)) {
        lockedReg(io.lockReq.payload) := cellState.locked
      }
    } elsewhen (io.unlockReq.valid) {
      when(lockCounts.dec(io.unlockReq.payload)) {
        lockedReg(io.unlockReq.payload) := cellState.clearing
      }
    }
  }

  // Pop Ctrl
  io.popReq.valid := False
  when (io.popReq.fire) {
    val isEmpty = waitCounts.dec(io.popReq.payload)
    when (isEmpty) {
      lockedReg(io.popReq.payload) := cellState.idle
      io.freeAddr.push(io.popReq.payload)
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
  val waitFifo   = new StreamFifo(inputCtrl.beforeWait.payloadType, numWaits)

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

  val lockAddrCtrl = new Area {
    // TODO: this is arrow.
    val Seq(existingLock, newLock) = inputCtrl.newLock.demux(inputCtrl.waitPort)

    // Bind this two fifos
    val newCmd = ids.io.alloc >*< newLock.fmap(_.snd)
    val nextCmd = inputCtrl.newLock.mux(existingLock, newCmd)

    nextCmd >> waitFifo.io.push
    nextCmd.fmap(_.fst).toFlow >> lock.io.lockReq
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

    cam.io.wr << ReturnFlow (writeCmd, insertReq.valid || deleteReq.valid)

    // Return of address
    ids.io.free <-< deleteReq.throwWhen(insertReq.valid)
  }

  val outputCtrl = new Area {
    // unlock forward
    io.unlock >> lock.io.unlockReq

    // output path
    val unlockLast = ~lock.io.isLocked(waitFifo.io.pop.fst)
    io.res << StreamArbiterFactory.onArgs(bypassFifo.io.pop, waitFifo.io.pop.continueWhen(unlockLast))
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
    val req = slave  Stream (AccessCommand(48))
    val dataIn  = slave Stream UInt(512 bits)
    val dataOut = master Stream UInt(512 bits)

    val dataWr = master Stream UInt(512 bits)
    val dataRd = slave Stream UInt(512 bits)

    // We use a bus mod!
    val bus = master (Axi4(Axi4Config(32, 32)))
  }

  val Seq(headerIfc, dataIfc) = StreamDemux(io.dataIn, inputFsm.isActive(inputFsm.busyWrite).asUInt, 2)
  dataIfc >> io.dataWr

  val lookback = Stream(UInt(512 bits))
  lookback.continueWhen(inputFsm.forward)

  val rdStream = StreamMux(inputFsm.isActive(inputFsm.busyRead).asUInt, Seq(lookback, io.dataRd))
  rdStream >> io.dataOut

  // State machine here
  // TODO: we can split this FSM into 2 fsms
  val inputFsm = new StateMachine {
    val counter = UInt(8 bits)
    val forward = Bool


    val init= new State with EntryPoint {
      whenIsActive {
        // if this is valid
        // if read,
        // if write,
        // if this is invalid
      }
      onExit {
        counter := getBeatsHeader(io.dataIn.payload)
      }
    }

    val busyWrite = new State {
      whenIsActive {
        when (io.dataIn.fire) {
          counter := counter - 1
          when (counter === 1) {
            goto (init)
          }
        }
      }
    }

    val busyRead = new State {
    }
  }

  def getSeqIdFromHeader(header: UInt): UInt = {
    header (7 downto 0)
  }
  def getBeatsHeader(header: UInt): UInt = {
    header (7 downto 0)
  }
}


class CoreUnit extends Component {
  val io = new Bundle {
    val ifc = master (LegoMemComponentInterface(LegoMemConfig()))
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

    val bramCache = new PTECache(tagWidth, addrWidth, numCacheCells)
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

//Generate the MyTopLevel's Verilog
object MyTopLevelVerilog {
  def main(args: Array[String]) {
    SpinalVerilog(new MemoryRegisterInterface(32))
  }
}

//Generate the MyTopLevel's VHDL
object MyTopLevelVhdl {
  def main(args: Array[String]) {
    SpinalVhdl(new MyTopLevel)
  }
}


//Define a custom SpinalHDL configuration with synchronous reset instead of the default asynchronous one. This configuration can be resued everywhere
object MySpinalConfig extends SpinalConfig(defaultConfigForClockDomains = ClockDomainConfig(resetKind = SYNC))

//Generate the MyTopLevel's Verilog using the above custom configuration.
object MyTopLevelVerilogWithCustomConfig {
  def main(args: Array[String]) {
    MySpinalConfig.generateVerilog(new MyTopLevel)
  }
}