package wuklab

import spinal.core._
import spinal.lib._
import Utils._

import scala.collection.mutable
import scala.collection.mutable.ArrayBuffer

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
    // This is one time operation
    val lockReq   = slave Flow UInt(cellWidth bits)
    val unlockReq = slave Flow UInt(cellWidth bits)

    // The freed address
    val freeAddr  = master Flow UInt(cellWidth bits)
    val isLocked  = out Vec(Bool, numCells)

    // Pop from queue will trigger pop req
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

// This is like a P4 module: match and action.
// We need Lens or sth to change a field in
// A -> change a field of A -> A

// Takes the control command, and then execute this
// mainly about: lock and unlock

trait MatchActionFunction {
  def cond (bits : Bits) : Bool
  def action (bits : Bits, valid : Bool) : Bits
}

abstract class MatchActionComponent extends Component with MatchActionFunction {
  val io = new Bundle {
    val ctrlIn  = slave Stream ControlRequest()
    val ctrlOut = master Stream ControlRequest()
  }
}

class DecodeAction extends MatchActionFunction {
  override def cond(bits : Bits) = True

  override def action(bits : Bits, valid : Bool) = LegoMemHeader.assignToBitsOperation(header => {
    when (header.reqType(7) === True) {
      header.cont := U"16'h00_00_00_02"
    } otherwise {
      switch (header.reqType) {
        is (LegoMem.RequestType.READ)  { header.cont := U"16'h00_00_00_01" }
        is (LegoMem.RequestType.WRITE) { header.cont := U"16'h00_00_00_01" }
        is (LegoMem.RequestType.PINGPONG) { header.cont := U"16'h00_00_00_06" }
        default { header.cont := U"16'h00_00_00_00" }
      }
    }
  })(bits)
}

class SequencerAction extends MatchActionFunction {

  override def cond(bits: Bits) = True
  override def action(bits: Bits, valid : Bool) = {
    val seqIdWidth = 8
    val counter = Counter(seqIdWidth bits, valid)
    val func = LegoMemHeader.assignToBitsOperation(_.seqId := counter.value)
    func(bits)
  }
}

// We need a match and action table here
// TODO: get rid of this java like thing
class MatchActionTableFactory {
  // Input stream
  val functions = ArrayBuffer[(Bits => Bool, (Bits, Bool) => Bits)]()
  val components = ArrayBuffer[(MatchActionComponent, Int)]()
  def addAction(f : MatchActionFunction) : Unit = functions += ((f.cond, f.action))
  def addComponent (c : MatchActionComponent, ctrlAddr : Int) : Unit = {
    components += ((c, ctrlAddr))
    functions += ((c.cond, c.action))
  }


  // TODO: look for a delayed init, refer to xbar
  // see https://github.com/SpinalHDL/SpinalHDL/blob/c40aa7df065f89a3c6ccd6bddb5bcfb2ae682adf/lib/src/main/scala/spinal/lib/bus/misc/BusSlaveFactory.scala#L684
  def build() = new Component {

    val io = new Bundle {
      val ctrlIn  = slave Stream ControlRequest()
      val ctrlOut = master Stream ControlRequest()
      val dataIn = slave Stream Fragment(Bits(512 bits))
      val dataOut = master Stream Fragment(Bits(512 bits))
    }

    // TODO: check the timing and maybe add the pipeline
    // The MAT tables
    io.dataOut << io.dataIn
    val res = functions.foldLeft(io.dataIn.fragment) { (bits, p) => Mux(p._1(bits), p._2(bits, io.dataOut.firstFire), bits) }
    when(io.dataIn.isFirst) { io.dataOut.fragment := res }

    // The ctrl path
    val ctrlValids = Vec(components.map(_._2 === io.ctrlIn.addr))
    val inputCtrls = StreamDemux(io.ctrlIn.takeWhen(ctrlValids.orR), OHToUInt(ctrlValids), components.size)
    (components, inputCtrls).zipped map (_._1.io.ctrlIn << _)

    // Add special for non-components
    if (components.nonEmpty)
      io.ctrlOut << StreamArbiterFactory.sequentialOrder.on(components.map(_._1.io.ctrlOut))
    else {
      io.ctrlOut << ReturnStream(cloneOf(io.ctrlOut.payload).clearAll, False)
    }
  }
}

// Data Flow:
// Input -> Headers -> Judge Pipeline -> Judge Stream    -> Buffer -> \
//       \> All     -> Buffer FIFO                     / -> Output -> - > Merge

// if it is a new lock, it will add carry the info in the tag field

// Look on every request's header.
// TODO: check FIFOs in this module
class FlowLockFactory(implicit config : CoreMemConfig) {
  type checkFunction = Bits => (Bits, Bits)
  type detectFunction = Bits => Bool
  type OpCode = UInt
  type sequenceFunction = (checkFunction, detectFunction, OpCode)
  val functions = ArrayBuffer[sequenceFunction]()

  // The tag width of the function
  val tagWidth : Int = 80
  val numWaits : Int  = 32
  val numCells : Int  = 32

  // Functions
  def addFunction (f : sequenceFunction) = functions += f

  def build = new Component {

    assert(isPow2(numCells))
    val cellWidth = log2Up(numCells)

    // Sequencer Function & Components

    val io = new Bundle {
      // LegoMem Data Interface
      val req = slave  Stream Fragment (Bits(512 bits))
      val res = master Stream Fragment (Bits(512 bits))

      val unlock = slave Flow UInt(cellWidth bits)
    }

    // Write : Match -> If exists, add; else, insert
    val cam = new LookupTCamStoppable(tagWidth, cellWidth, true)
    val ids = new IDPool(numCells)
    val lock = new LockCounterRam(numWaits, numCells)

    // we also assign sequence here
    val waitFifoSize = numWaits * 16
    val waitFifo = StreamFifo(Fragment(Bits(512 bits)), waitFifoSize)

    val unlockLast = ~lock.io.isLocked(LegoMemHeader(waitFifo.io.pop.payload.asBits).tag)
    val inputStream = StreamArbiterFactory
      .lowerFirst
      .onArgs(waitFifo.io.pop.continueWhen(unlockLast), io.req)

    // Data flow start
    val lookupLatency = 10
    val dataStream = cam.streamDelay(inputStream)

    val checkLockCtrl = new Area {
      val (headerStreamF, dataStreamF) = StreamFork2(dataStream)
      val headerStream = headerStreamF.takeWhen(headerStreamF.first).fmap(_.fragment)

      val infoStream = checkLock(headerStream)

      cam.io.rd.req <<  infoStream.fmap(_.tag.asUInt |> LookupReadReq.apply)

      val (toLockF, signal) = StreamFork2(cam.io.rd.res)
      val toLock = toLockF.takeBy(_.hit).fmap(_.value)

      val (waitStreamF, continueStream) = dataStreamF.forkBySignal(signal.fmap(_.hit))
      // TODO: check timing order here
      val waitStream = dataStreamF.fmapFirst (
        LegoMemHeader.assignToBitsOperation ( _.tag := signal.value ))
      waitFifo.io.push << waitStream
    }

    // Data path
    val lookupCtrl = new Area {
      val (headerStreamF, dataStreamF) = StreamFork2(checkLockCtrl.continueStream)
      val headerStream = headerStreamF.takeWhen(headerStreamF.first).fmap(_.fragment)

      // lookup
      // if we want a fine solution, we could just add a side comparison here
      val resultStreamF = detectLock(headerStream) >*< ReturnStream(U"8'h0")

      // TODO: Assign lock addr for this
      // TODO: add an abstraction for this 3 way merge
      val newLock = resultStreamF.fst.lock
      val resultStream = cloneOf(resultStreamF)
      resultStream.payload := resultStreamF.payload
      when (newLock) { resultStream.snd := ids.io.alloc.payload }
      resultStream.valid := resultStreamF.valid && Mux(newLock, ids.io.alloc.valid, True)
      resultStreamF.ready := resultStream.ready && Mux(newLock, ids.io.alloc.valid, True)
      ids.io.alloc.ready := newLock && resultStream.ready && resultStreamF.valid

      // ======= Output =========
      // Send info to 3 different locations
      val Seq(toTagF, toLockF, toCamF) = StreamFork(resultStream, 3)

      val toTag = toTagF.fmap(_.fst.tag)
      // If theres a lock or we need a new lock, we need to send an lock instruction
      val toLock = toLockF.takeBy(p => p.fst.lock).fmap(_.snd)
      // We only send if we need a new lock
      val toCam = toCamF.takeBy(_.fst.lock)

      // Process data stream
      val outputStream = dataStreamF.fmapFirst (
        LegoMemHeader.assignToBitsOperation ( _.tag := toTag.payload.asUInt ))
    }

    // We merge the wait flow (if unlocked) and the continue flow
    io.res << lookupCtrl.outputStream

    // The lock module processes lock and unlock
    // - lock will be selected from 1) initial a new lock 2) reduce a existing lock
    // - unlock will be from the external unlock request
    //    - The unlock logic should lookup the tag field
    val lockCtrl = new Area {
      // TODO: check the toFlow here
      lock.io.lockReq << lookupCtrl.toLock.toFlow
      lock.io.unlockReq << io.unlock
      lock.io.popReq << waitFifo.io.pop
        .tapAsFlow
        .takeWhen(waitFifo.io.pop.first)
        .fmap(m => LegoMemHeader(m.fragment).tag)
    }

    // This part looks great
    val camCtrl = new Area {
      // This is an address delete req bypass structure
      // only lock for new ones
      val insertReq = lookupCtrl.toCam
      val deleteReq = lock.io.freeAddr

      val writeCmd = cloneOf (cam.io.wr.payload)
      // Set the command
      // For Now, the only thing we can do is lock on PID
      writeCmd.key    := insertReq.fst.tag  .asUInt
      writeCmd.mask   := insertReq.fst.mask .asUInt
      writeCmd.value  := Mux(deleteReq.valid && insertReq.valid, deleteReq.payload, insertReq.snd)
      // If is not insert, then used is false (it is a delete)
      writeCmd.enable := insertReq.valid

      cam.io.wr << ReturnFlow(writeCmd, insertReq.valid || deleteReq.valid)
      // Return of address
      ids.io.free <-< deleteReq.throwWhen(insertReq.valid)
    }

    case class LockInfo() extends Bundle {
      val tag  = Bits(tagWidth bits)
      val mask = Bits(tagWidth bits)
      val lock = Bool
    }

    def checkLock(req : Stream[Bits]) : Stream[LockInfo] = {
      req.fmap { header =>
        val (keys, masks) = functions
          .map(_._1)
          .map(header |> _)
          .unzip
        val bits = functions.map(LegoMemHeader(header).reqType === _._3)

        val next = LockInfo()
        next.tag  := MuxOH(Vec(bits), keys)
        next.mask := MuxOH(Vec(bits), masks)
        next.lock := False
        next
      }
    }
    def detectLock(req : Stream[Bits]) : Stream[LockInfo] = {
      req.fmap { header =>
        val (keys, masks) = functions
          .map(_._1)
          .map(header |> _)
          .unzip
        val bits = functions.map
          { case (_, f, op) => LegoMemHeader(header).reqType === op && f(header) }

        val next = LockInfo()
        next.tag := MuxOH(Vec(bits), keys)
        next.mask := MuxOH(Vec(bits), masks)
        next.lock := bits.reduce(_ || _)
        next
      }
    }
  }

}

class MemoryModelController extends Component {
  val io = new Bundle {
    val src = slave (LegoMemControlEndpoint())

    // control interface
    val sub = new Bundle {
      val mat = master (LegoMemControlEndpoint())
    }
    val raw = new Bundle {
      val netSession = master Stream UInt(16 bits)
    }
  }

  // input arbiter
  val Seq(toSess, toMat) = StreamDemux(io.src.in, io.src.in.addr(0).asUInt, 2)
  toMat >> io.sub.mat.in
  io.raw.netSession << toSess.fmap (_.param32(15 downto 0))

  // output arbiter
  io.sub.mat.out >> io.src.out
}

class MemoryModel(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val net = NetworkInterface()
    val sess = master Stream UInt(16 bits)
  }

  val bridge = new RawInterfaceEndpoint
  bridge.io.ep <> io.ep

  // Controller
  val controller = new MemoryModelController
  controller.io.src.in << bridge.io.raw.ctrlIn
  controller.io.src.out >> bridge.io.raw.ctrlOut
  controller.io.raw.netSession >> io.sess

  // network adapter
  val net = new NetworkAdapter
  net.io.net <> io.net
  net.io.seq.dataIn << bridge.io.raw.dataIn

  // Match Action Table
  val matBuilder = new MatchActionTableFactory
  matBuilder.addAction(new SequencerAction)
  matBuilder.addAction(new DecodeAction)

  val mat = matBuilder.build()
  mat.io.dataOut >> bridge.io.raw.dataOut
  mat.io.dataIn << net.io.seq.dataOut
  mat.io.ctrlIn << controller.io.sub.mat.in
  mat.io.ctrlOut >> controller.io.sub.mat.out

  // val lockBuilder = new FlowLockFactory
  // val lock = lockBuilder.build
  // lock.io.req << mat.io.dataOut
  // lock.io.res >> bridge.io.raw.dataOut
  // // TODO: fix this
  // lock.io.unlock << bridge.io.raw.dataIn.tapAsFlow.fmap { f => LegoMemHeader(f.fragment).tag }
}
