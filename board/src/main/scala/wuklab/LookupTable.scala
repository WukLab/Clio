package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.lib._


case class LookupResult() extends Bundle {
  val pa = UInt(32 bits)
  val pte = PTE
  val isMatch = Bool
  val cell = UInt(16 bits)
}

case class LookupWrite(keyWidth : Int, valueWidth : Int,
                       useMask : Boolean = true,
                       useEnable : Boolean = false,
                       useWriteValid : Boolean = false
                      ) extends Bundle {
  val mask  = if (useMask) UInt(keyWidth bits) else null
  val enable = if (useEnable) Bool else null
  val used = if (useWriteValid) Bool else null
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

class LookupTCam(keyWidth : Int, valueWidth : Int) extends Component with NonStopablePipeline {
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

  val wr = Flow (LookupWrite(keyWidth, valueWidth, useMask, useEnable, useWriteValid))
  val rd = new Bundle with IMasterSlave {
    val req = Stream (LookupReadReq(keyWidth))
    val res = Stream (LookupReadRes(valueWidth))

    override def asMaster(): Unit = {
      master (req)
      slave (res)
    }
  }

}

class LookupTCamStoppable(keyWidth : Int, valueWidth : Int, useWriteValid : Boolean) extends Component with StoppablePipeline {

  override def pipelineDelay: Int = 1
  override def enableSignal : Bool = io.rd.res.ready

  val io = slave (LookupStreamReadInterface(keyWidth, valueWidth, true, false, useWriteValid))

  val dut = new TernaryCAM(keyWidth, valueWidth, true, useWriteValid)
  dut.io.wren := io.wr.valid
  dut.io.wrmask := io.wr.mask
  dut.io.wrkey := io.wr.key
  dut.io.wraddr := io.wr.value
  if (useWriteValid) dut.io.wrvalid := io.wr.used

  dut.io.rdkey := io.rd.req.key

  io.rd.res.value := dut.io.rdaddr
  io.rd.res.hit := dut.io.rdvalid
  io.rd.res.valid := RegNextWhen(io.rd.req.valid, enableSignal) init False

  io.rd.req.ready := enableSignal
  dut.io.enable := enableSignal

}

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
    if (useEnable) RegNextWhen(valid, io.enable) else RegNext(valid)
  }

}

