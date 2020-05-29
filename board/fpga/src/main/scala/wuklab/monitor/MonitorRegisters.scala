package wuklab.monitor

import wuklab._
import spinal.core._
import spinal.core.internals.Operator
import spinal.lib._
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Config, AxiLite4SlaveFactory}

// TODO: generate Reg MAP

class MonitorRegisters(numInputs : Int, numOutputs : Int, baseAddr : BigInt) extends Component with XilinxAXI4Toplevel {

  def enableInputs = numInputs > 0
  def enableOutputs = numOutputs > 0
  def offset(i : Int) = baseAddr + BigInt(i * 4)

  val io = new Bundle {
    // Debug Interfaces
    val inputRegs  = if (enableInputs)  in  Vec(UInt(32 bits), numInputs)  else null
    val outputRegs = if (enableOutputs) out Vec(UInt(32 bits), numOutputs) else null

    val regBus = slave (AxiLite4(AxiLite4Config(32, 32)))
  }

  val regs = new AxiLite4SlaveFactory(io.regBus)
  if (enableOutputs) io.outputRegs.zipWithIndex.map { case (out, i) =>
    val reg = Reg (UInt(32 bits)) init 0xdead
    regs.readAndWrite(reg, offset(i))
    out := reg
  }
  if (enableInputs)  io.inputRegs .zipWithIndex.map { case (reg, i) => regs.read(reg, offset(numOutputs + i)) }

  addPrePopTask(renameIO)
}
