package wuklab.test

import spinal.core._
import spinal.core.sim._

import wuklab._
import wuklab.sim._

import SimContext._

object LegoMemSystemCtrlSim {
  import SimConversions._
  def main(args: Array[String]): Unit = {
    LegoMemSimConfig.doSim(
      new LegoMemSystem
    ) {dut => {
      dut.clockDomain.forkStimulus(5)

      val xbar = new LegoMemEndPointInterconnect(dut.clockDomain)
      xbar.=# (0) (dut.io.eps.seq)
      xbar.=# (1) (dut.io.eps.mem)
      xbar.=# (2) (dut.io.eps.soc)

      // Connect Interfaces to Memories
      val mem = new DictMemoryDriver(dut.clockDomain)
      mem =# dut.io.bus.access
      mem =# dut.io.bus.lookup
      val socCtrlStream = new StreamDriver(dut.io.soc.ctrlOut, dut.clockDomain)

      dut.clockDomain.assertReset()
      dut.clockDomain.waitRisingEdge(5)
      dut.clockDomain.deassertReset()

      dut.clockDomain.waitRisingEdge(10)
      println("Initializtion finish")

      dut.clockDomain.waitRisingEdge(80)
    }}

  }
}
