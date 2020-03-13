package wuklab

// System Top Level Class

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4

class LegoMemSystem(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    // End Points to Cross bar
    val eps = new Bundle {
      val seq = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
      val mem = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
      val soc = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    }

    // Other external interface
    val net = NetworkInterface()
    val soc = master (LegoMemRawInterface())
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
  soc.io.raw <> io.soc

}

