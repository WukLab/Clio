package wuklab.monitor

import wuklab._
import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Config, AxiLite4SlaveFactory}
import wuklab.{AxiStreamConfig, AxiStreamPayload}
import Utils._

class NetworkLatencyCounter extends Component with XilinxAXI4Toplevel {
  val headerWidth = 112
  val netConfig = AxiStreamConfig(64, keepWidth = 8, userWidth = 1)
  val io = new Bundle {
    val rx = new Bundle {
      val headerIn  = slave Stream Bits(headerWidth bits)
      val headerOut = master Stream Bits(headerWidth bits)

      val dataIn    = slave Stream Fragment(AxiStreamPayload(netConfig))
      val dataOut   = master Stream Fragment(AxiStreamPayload(netConfig))
    }

    val tx = new Bundle {
      val headerIn  = slave Stream Bits(headerWidth bits)
      val headerOut = master Stream Bits(headerWidth bits)

      val dataIn    = slave Stream Fragment(AxiStreamPayload(netConfig))
      val dataOut   = master Stream Fragment(AxiStreamPayload(netConfig))
    }

    val regBus = slave (AxiLite4(AxiLite4Config(8, 32)))
  }

  val counter = Counter(32 bits, True)

  def filter(data : Bits) = data
}

class NetworkProtocolChecker(baseAddr : BigInt) extends Component with XilinxAXI4Toplevel {
  val headerWidth = 112
  val netConfig = AxiStreamConfig(64, keepWidth = 8, userWidth = 1)

  val io = new Bundle {
    val headerIn  = slave Stream Bits(headerWidth bits)
    val headerOut = master Stream Bits(headerWidth bits)

    val dataIn    = slave Stream Fragment(AxiStreamPayload(netConfig))
    val dataOut   = master Stream Fragment(AxiStreamPayload(netConfig))

    val regBus = slave (AxiLite4(AxiLite4Config(8, 32)))
  }

  io.headerOut <-< io.headerIn
  io.dataOut <-< io.dataIn

  // Protocol Checker
  val errorBytes = Reg (UInt(32 bits)) init 0
  val errorDataCount = Reg (UInt(32 bits)) init 0

  val totalHeaders = Reg (UInt(32 bits)) init 0
  val totalLasts = Reg (UInt(32 bits)) init 0
  val totalHeaderBytes = Reg (UInt(32 bits)) init 0
  val totalDataBytes = Reg (UInt(32 bits)) init 0

  when (io.headerIn.fire) {
    totalHeaders := totalHeaders + 1
    totalHeaderBytes := totalHeaderBytes + headerToSize(io.headerIn.payload)
  }

  val curDataBytes = Reg (UInt(32 bits)) init 0
  when (io.dataIn.fire) {
    val nextDataBytes = curDataBytes + keepToBytes(io.dataIn.tkeep)
    when (io.dataIn.last) {
      totalLasts := totalLasts + 1
      totalDataBytes := totalDataBytes + nextDataBytes
      // Check the Bytes
      curDataBytes := 0
    } otherwise {
      curDataBytes := nextDataBytes
    }
  }

  // Slave bus
  val regs = new AxiLite4SlaveFactory(io.regBus)
  def offset(i : Int) = baseAddr + i * 4
  regs.read(totalHeaders,     offset(0))
  regs.read(totalLasts,       offset(1))
  regs.read(totalDataBytes,   offset(2))
  regs.read(totalHeaderBytes, offset(3))

  def headerToSize(header: Bits) : UInt = header(96, 16 bits).asUInt
  def keepToBytes(tkeep: Bits) : UInt = OHToUInt(tkeep.asUInt +^ 1)

  addPrePopTask(renameIO)
}
