package wuklab

// System Top Level Class

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4

import Utils._

class LegoMemSystem(implicit config : CoreMemConfig) extends Component with XilinxAXI4Toplevel {
  val io = new Bundle {
    // End Points to Cross bar
    val eps = new Bundle {
      val seq = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
      val mem = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
      val soc = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    }

    // Other external interface
    val net = NetworkInterface()
    val soc = new Bundle {
      val dataIn  = master Stream Fragment(Bits(512 bits))
      val dataOut = slave Stream Fragment(Bits(512 bits))
      val ctrlIn  = master Stream (Bits(64 bits))
      val ctrlOut = slave Stream (Bits(64 bits))
    }
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }

  }

  val sequencer = new MemoryModel
  sequencer.io.ep <> io.eps.seq
  sequencer.io.net <> io.net

  val access = new MemoryAccessEndPoint
  access.io.ep <> io.eps.mem
  access.io.bus <> io.bus

  val soc = new RawInterfaceEndpoint
  soc.io.ep <> io.eps.soc
  soc.io.raw.dataIn >> io.soc.dataIn
  soc.io.raw.dataOut << io.soc.dataOut
  io.soc.ctrlIn << soc.io.raw.ctrlIn.fmap(_.asBits)
  soc.io.raw.ctrlOut.translateFrom (io.soc.ctrlOut) (_.assignFromBits(_))

  addPrePopTask(renameIO)

}

