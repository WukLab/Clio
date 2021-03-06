package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.core.internals.Operator
import spinal.lib._

case class LookupWrite(keyWidth : Int, valueWidth : Int,
                       useMask : Boolean = true,
                       useEnable : Boolean = false,
                       useUsed : Boolean = false,
                       usedWidth : Int = 0
                      ) extends Bundle {
  assert(!useUsed || usedWidth != 0)

  val mask  = if (useMask) UInt(keyWidth bits) else null
  val enable = if (useEnable) Bool else null
  val used = if (useUsed) Bool else null
  val usedCell = if (useUsed) UInt(usedWidth bits) else null
  val key   = UInt(keyWidth bits)
  val value = UInt(valueWidth bits)
}

object LookupReadReq {
  def apply(payload : UInt): LookupReadReq = {
    val req = apply(payload.getWidth)
    req.key := payload
    req
  }
}
case class LookupReadReq (keyWidth : Int) extends Bundle {
  val key = UInt(keyWidth bits)
}

object LookupReadRes {
  def apply(flow : Flow[UInt]): LookupReadRes = {
    val next = apply(flow.payload.getWidth)
    next.hit := flow.valid
    next.value := flow.payload
    next
  }
}
case class LookupReadRes (valueWidth : Int) extends Bundle {
  val hit   = Bool
  val value = UInt(valueWidth bits)
}

trait LookupInterface extends Bundle with IMasterSlave {
  val keyWidth : Int
  val valueWidth : Int
  val useMask : Boolean
  val useEnable : Boolean
  val useWriteValid : Boolean

  val wr : IMasterSlave
  val rd : IMasterSlave

  override def asMaster(): Unit = {
    master (wr)
    master (rd)
  }
}

case class LookupFlowInterface(keyWidth : Int,
                               valueWidth : Int,
                               useMask : Boolean,
                               useEnable : Boolean,
                               useWriteValid: Boolean = false
                              ) extends LookupInterface {
  val wr = Flow (LookupWrite(keyWidth, valueWidth, useMask, useEnable))
  val rd = new Bundle with IMasterSlave {
    val req = Flow (LookupReadReq(keyWidth))
    val res = Flow (LookupReadRes(valueWidth))

    override def asMaster(): Unit = {
      master (req)
      slave (res)
    }
  }
}

class LookupTCam(keyWidth : Int, valueWidth : Int) extends Component with NonStoppablePipeline {
  val io = slave (LookupFlowInterface(keyWidth, valueWidth, true, false))

  val dut = new TernaryCAM(keyWidth, valueWidth)
  dut.io.wren := io.wr.valid
  dut.io.wrmask := io.wr.mask
  dut.io.wrkey := io.wr.key
  dut.io.wraddr := io.wr.value

  dut.io.rdkey := io.rd.req.key

  io.rd.res.value := dut.io.rdaddr
  io.rd.res.hit := dut.io.rdvalid
  io.rd.res.valid := Delay(io.rd.req.valid, dut.readDelay)

  override def pipelineDelay: Int = dut.readDelay
}

case class LookupStreamReadInterface(keyWidth : Int,
                                     valueWidth : Int,
                                     useMask : Boolean,
                                     useEnable : Boolean,
                                     useWriteValid : Boolean
                                    ) extends LookupInterface {

  val wr = Flow (LookupWrite(keyWidth, valueWidth, useMask, useWriteValid))
  val rd = new Bundle with IMasterSlave {
    val req = Stream (LookupReadReq(keyWidth))
    val res = Stream (LookupReadRes(valueWidth))

    override def asMaster(): Unit = {
      master (req)
      slave (res)
    }
  }

}

trait RequestResponseInterface extends IMasterSlave {
  val req : IMasterSlave
  val res : IMasterSlave

  override def asMaster(): Unit = {
    master(req)
    slave (res)
  }
}

case class LookupStreamInterface(
                                  keyWidth : Int,
                                  valueWidth : Int,
                                  cellWidth : Int,
                                  useMask : Boolean
                                ) extends LookupInterface {

  override val useEnable = true
  override val useWriteValid = false

  val wr = new Bundle with IMasterSlave {
    val req = Stream (LookupWrite(keyWidth, valueWidth, useMask, useWriteValid))
    val res = Flow (UInt(cellWidth bits))
    val shootDown = Flow (UInt(cellWidth bits))

    override def asMaster(): Unit = {
      master (req)
      slave (res)
      master (shootDown)
    }
  }
  val rd = new Bundle with RequestResponseInterface {
    val req = Stream (LookupReadReq(keyWidth))
    val res = Stream (LookupReadRes(valueWidth))
  }
}

class LookupTCamStoppable(keyWidth : Int, valueWidth : Int, useEnable : Boolean) extends Component with StoppablePipeline {

  override def pipelineDelay: Int = 1
  override def enableSignal : Bool = io.rd.res.ready || !io.rd.res.valid

  val io = slave (LookupStreamReadInterface(keyWidth, valueWidth, true, false, useEnable))

  val dut = new TernaryCAM(keyWidth, valueWidth, true, useEnable)
  dut.io.wren := io.wr.valid
  dut.io.wrmask := io.wr.mask
  dut.io.wrkey := io.wr.key
  dut.io.wraddr := io.wr.value
  if (useEnable) dut.io.wrvalid := io.wr.enable

  dut.io.rdkey := io.rd.req.key

  io.rd.res.value := dut.io.rdaddr
  io.rd.res.hit := dut.io.rdvalid
  io.rd.res.valid := RegNextWhen(io.rd.req.valid, enableSignal) init False

  io.rd.req.ready := enableSignal
  dut.io.enable := enableSignal

}

class LookupTableStoppable(
                            keyWidth : Int,
                            valueWidth : Int,
                            numCells : Int,
                            useUsed : Boolean = false,
                            autoUpdate : Boolean = false,
                            useHitReport : Boolean = false
                          ) extends Component with StoppablePipeline {
  assert(if (autoUpdate) useUsed else true, "AutoUpdate relies on used information.")

  assert(isPow2(numCells))
  val cellWidth = log2Up(numCells)

  override def pipelineDelay : Int = 2
  override def enableSignal : Bool = io.rd.res.ready || !io.rd.res.valid

  val io = new Bundle {
    val wr = new Bundle {
      val req = slave Stream LookupWrite(keyWidth, valueWidth, useUsed = useUsed, usedWidth = cellWidth)
      val res = master Flow UInt(cellWidth bits)
      val shootDown = slave Flow UInt(cellWidth bits)
    }
    val rd = new Bundle {
      val req = slave Stream LookupReadReq(keyWidth)
      val res = master Stream LookupReadRes(valueWidth)
      val rpt = if (useHitReport) master Flow UInt(cellWidth bits) else null
    }

  }

  val cam = new LookupTCamStoppable(keyWidth, cellWidth, true)
  val ids = new IDPool(numCells)
  val mem = new Mem(Bits(valueWidth bits), numCells)

  // This part generate a write stream, select from UPDATE and INSERT
  val wrCell = Stream(UInt(cellWidth bits))
  if (useUsed) {
    // If enable autoUpdate, we will auto fetch the used info.
    val isUpdate = if (autoUpdate) !ids.io.alloc.valid else io.wr.req.used
    io.wr.req.ready := Mux(isUpdate, wrCell.ready, wrCell.ready && ids.io.alloc.valid)
    ids.io.alloc.ready := !isUpdate && wrCell.ready && io.wr.req.valid
    wrCell.valid := Mux(isUpdate, io.wr.req.valid, io.wr.req.valid && ids.io.alloc.valid)
    wrCell.payload := Mux(isUpdate, io.wr.req.usedCell, ids.io.alloc.payload)
  } else {
    wrCell << (io.wr.req *> ids.io.alloc)
  }

  val addrFlow = StreamFlowArbiter(wrCell, io.wr.shootDown)
  cam.io.wr << addrFlow.fmap(cell => {
    val next = cloneOf(cam.io.wr.payload)
    next.key := io.wr.req.key
    next.mask := io.wr.req.mask
    next.enable := wrCell.valid
    next.value := cell
    next
  })

  // write to data, stage for 1 cycle
  val delayedValue = RegNext(io.wr.req.value.asBits)
  val delayedValid = RegNext(wrCell.valid) init False
  val delayedAddr = RegNext(wrCell.payload)
  mem.write(delayedAddr, delayedValue, delayedValid)

  ids.io.alloc.tapAsFlow >> io.wr.res
  io.wr.shootDown >> ids.io.free

  val readCtrl = new {
    //Read path. stage 1
    io.rd.req >> cam.io.rd.req
    //Read path. stage 2
    io.rd.res.hit := RegNextWhen(cam.io.rd.res.hit, enableSignal) init False
    io.rd.res.valid := RegNextWhen(cam.io.rd.res.valid, enableSignal) init False
    io.rd.res.value := mem.readSync(cam.io.rd.res.value).asUInt
    cam.io.rd.res.ready := enableSignal
    // report path
    if (useHitReport)
      io.rd.rpt << cam.io.rd.res.tapAsFlow.takeBy(_.hit).stage().fmap(_.value)
  }

}

// Enable for stall,
// Write Valid for Write Valid
class TernaryCAM(keyWidth : Int, addrWidth : Int,
                 useEnable : Boolean = false, useWriteValid : Boolean = false) extends Component {

  // Parameter
  val readDelay : Int = 1

  val io = new Bundle {
    val wrkey  = in UInt(keyWidth bits)
    val wrmask = in UInt(keyWidth bits)
    val wraddr = in UInt(addrWidth bits)
    val wrvalid = if (useWriteValid) in Bool else null
    val wren   = in Bool

    val rdkey  = in UInt(keyWidth bits)
    val rdaddr = out UInt(addrWidth bits)
    val rdvalid = out Bool

    val enable  = if (useEnable) in Bool else null
  }

  val numCells = 1 << addrWidth
  val keyRegs, maskRegs = Vec(Reg(UInt(keyWidth bits)), numCells)
  val used = Vec(Reg (Bool) init False, numCells)

  when (io.wren) {
    keyRegs(io.wraddr) := io.wrkey
    maskRegs(io.wraddr) := io.wrmask
    used(io.wraddr) := (if (useWriteValid) io.wrvalid else io.wrmask.orR)
  }

  // TODO: check the timing of this path
  val validVec = Vec((keyRegs, maskRegs).zipped map matchCell)
  val resVec = validVec.asBits & used.asBits
  io.rdaddr := OHToUInt(resVec)
  io.rdvalid := resVec.orR

  def matchCell(key : UInt, mask : UInt) : Bool = {
    val valid = (~(key ^ io.rdkey) | ~mask).andR
    if (useEnable) RegNextWhen(valid, io.enable).init(False) else RegNext(valid).init(False)
  }

}
