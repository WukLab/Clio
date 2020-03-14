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

// This is like a P4 module: match and action.
// We need Lens or sth to change a field in
// A -> change a field of A -> A

// Takes the control command, and then execute this
// mainly about: lock and unlock
class ModelController extends Component {

}

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
    switch (header.reqType) {
      is (LegoMem.RequestType.READ)  { header.cont := U"16'h00_00_00_01" }
      is (LegoMem.RequestType.WRITE) { header.cont := U"16'h00_00_00_01" }
      is (LegoMem.RequestType.ALLOC) { header.cont := U"16'h00_00_00_01" }
      is (LegoMem.RequestType.FREE)  { header.cont := U"16'h00_00_00_01" }
      default { header.cont := U"16'h00_00_00_01" }
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


// class Sequencer(dataWidth : Int, tagWidth : Int, numWaits : Int, numCells : Int) extends Component {
//
//   assert(isPow2(numCells))
//   val cellWidth = log2Up(numCells)
//
//   val io = new Bundle {
//     val req = slave  Stream Fragment (Bits(512 bits))
//     val res = master Stream Fragment (Bits(512 bits))
//
//     val unlock = slave Flow UInt(cellWidth bits)
//   }
//
//   // Write : Match -> If exists, add; else, insert
//   val cam = new LookupTCamStoppable(dataWidth, cellWidth, true)
//   val ids = new IDPool(numCells)
//   val lock = new LockCounterRam(numWaits, numCells)
//
//   // Internal infomations
//   val bypassFifo = new StreamFifoLowLatency(io.req.payloadType, 1)
//
//   // Data path
//   val inputCtrl = new Area {
//
//     // flow through CAM
//     val afterLookup = requireLock(io.req)
//
//     // See if we need lock
//     val newLock = isLockInstruction(afterLookup.snd)
//     val requireLockBool =  afterLookup.fst.hit || newLock
//
//     // split to two flows
//     val beforeWait = afterLookup.fmapFst(_.value)
//     val Seq(bypassPort, waitPort) = StreamDemux(beforeWait, requireLockBool.asUInt, 2)
//     bypassPort.fmap(_.snd) >> bypassFifo.io.push
//   }
//
//   val waitFifo   = new StreamFifo(inputCtrl.beforeWait.payloadType, numWaits)
//
//   val lockAddrCtrl = new Area {
//     // TODO: this is arrow.
//     val Seq(existingLock, newLock) = inputCtrl.newLock.demux(inputCtrl.waitPort)
//
//     // Bind this two fifos
//     val newCmd = ids.io.alloc >*< newLock.fmap(_.snd)
//     val nextCmd = StreamMux(inputCtrl.newLock.asUInt, Seq(existingLock, newCmd))
//     //    val nextCmd =  inputCtrl.newLock.mux(existingLock, newCmd)
//
//     nextCmd >> waitFifo.io.push
//     nextCmd.tapAsFlow.fmap(_.fst) >> lock.io.lockReq
//   }
//
//   val camCtrl = new Area {
//     val insertReq = lockAddrCtrl.newCmd.tapAsFlow
//     val deleteReq = lock.io.freeAddr
//
//     val writeCmd = cloneOf (cam.io.wr.payload)
//
//     // Set the command
//     writeCmd.key := getTagFromRequest(insertReq.snd)
//     writeCmd.mask := getMaskFromRequest(insertReq.snd)
//     // If is not insert, then used is false (it is a delete)
//     writeCmd.enable := insertReq.valid
//     writeCmd.value := Mux(deleteReq.valid && insertReq.valid, deleteReq.payload, insertReq.payload.fst)
//
//     cam.io.wr << ReturnFlow(writeCmd, insertReq.valid || deleteReq.valid)
//
//     // Return of address
//     ids.io.free <-< deleteReq.throwWhen(insertReq.valid)
//   }
//
//   val outputCtrl = new Area {
//     // unlock forward
//     io.unlock >> lock.io.unlockReq
//     waitFifo.io.pop.tapAsFlow.fmap(_.fst) >> lock.io.popReq
//
//     // output path
//     val unlockLast = ~lock.io.isLocked(waitFifo.io.pop.fst)
//     io.res << StreamArbiterFactory.onArgs(
//       bypassFifo.io.pop,
//       waitFifo.io.pop.fmap(_.snd).continueWhen(unlockLast)
//     )
//   }
//
//   def getTagFromRequest (req: InternalMemoryRequest) : UInt = {
//     req.addr
//   }
//
//   def getMaskFromRequest (req: InternalMemoryRequest) : UInt = {
//     req.mask
//   }
//
//   def requireLock (req : Stream[InternalMemoryRequest]) = {
//     cam.io.rd.req << io.req.fmap(_ |> getTagFromRequest |> LookupReadReq.apply)
//
//     val cmd = cam.delay(req.payload)
//     cam.io.rd.res fmap (Pair(_, cmd))
//   }
//
//   def isLockInstruction (req : LegoMemHeader) : Bool = {
//     // TODO: make functions a class member
//     val functions = Seq[LegoMemHeader => (Bool, UInt)]()
//     val (bits, masks) = functions.map(req |> _).unzip
//
//     val mask = MuxOH(Vec(bits), masks)
//     val valid = bits.reduce(_ || _)
//
//     (valid, mask)
// //    req.reqType === MemoryRequestType.alloc || req.reqType === MemoryRequestType.free
//   }
//
// }

class MemoryModel(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val net = NetworkInterface()
  }

  val bridge = new RawInterfaceEndpoint
  bridge.io.ep <> io.ep

  // network adapter
  val net = new NetworkAdapter
  net.io.net <> io.net
  net.io.seq.dataIn << bridge.io.raw.dataIn

  // Match Action Table
  val matBuilder = new MatchActionTableFactory
  matBuilder.addAction(new SequencerAction)
  matBuilder.addAction(new DecodeAction)

  val mat = matBuilder.build()
  mat.io.dataIn << net.io.seq.dataOut
  mat.io.dataOut >> bridge.io.raw.dataOut
  mat.io.ctrlIn << bridge.io.raw.ctrlIn
  mat.io.ctrlOut >> bridge.io.raw.ctrlOut
}
