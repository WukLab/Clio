package wuklab

import wuklab.sim._
import spinal.core._
import spinal.sim._
import spinal.core.sim._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4CrossbarFactory}
import spinal.lib._

import scala.util.Random

object MemoryRegisterInterfaceSim {

  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new MemoryRegisterInterface(16)) { dut =>

      dut.clockDomain.forkStimulus(5)

      dut.clockDomain.waitRisingEdge(5)

      val bram = new Axi4SlaveMemoryDriver(dut.clockDomain, 1024)
      bram.init(
        (0, Seq[Byte](0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16) )
      )
      bram =# dut.io.memBus

      val regs = new AxiLite4RegDriver(dut.io.regBus, dut.clockDomain)

      dut.clockDomain.waitRisingEdge(5)

      regs.write(4, 3)
      regs.write(0, 1)

      dut.clockDomain.waitRisingEdge(5)

      regs.read(8)

      dut.clockDomain.waitRisingEdge(5)
    }
  }
}

object FetchUnitSim {

  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new FetchUnit(42)) { dut =>

      dut.clockDomain.forkStimulus(5)

      dut.clockDomain.waitRisingEdge(5)

      val bram = new Axi4SlaveMemoryDriver(dut.clockDomain, 4096)
      bram.init(
        (0, Seq[Byte](0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16) )
      )

      dut.io.req
    }
  }
}

object IDPoolSim {
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new IDPool(16)) { dut =>
      dut.clockDomain.forkStimulus(5)

      dut.io.alloc.ready #= false
      dut.io.free.valid #= false

      dut.clockDomain.waitRisingEdge(5)

      for (i <- 0 until 16) {
        dut.io.alloc.ready #= true
        dut.clockDomain.waitRisingEdge(1)
      }

      dut.clockDomain.waitRisingEdge(10)

      for (i <- Seq(4,7,3,8)) {
        dut.io.free.valid #= true
        dut.io.free.payload #= i
        dut.clockDomain.waitRisingEdge(1)
      }
      dut.io.free.valid #= false
      dut.clockDomain.waitRisingEdge(5)

    }
  }
}

object LookupTCamSim {
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new LookupTCam(32, 8)) { dut =>
      dut.io.wr.valid #= false
      dut.io.rd.req.valid #= false

      dut.clockDomain.forkStimulus(5)

      dut.clockDomain.waitRisingEdge(5)

      dut.io.wr.key   #= 0x4321
      dut.io.wr.mask  #= 0xFFF0
      dut.io.wr.value #= 7
      dut.io.wr.valid #= true

      dut.clockDomain.waitRisingEdge(1)
      dut.io.wr.valid #= false

      dut.io.rd.req.valid #= true
      dut.io.rd.req.key   #= 0x4322
      dut.clockDomain.waitRisingEdge(1)

      dut.io.rd.req.key   #= 0x4332
      dut.clockDomain.waitRisingEdge(1)
      dut.io.rd.req.valid #= false

      dut.clockDomain.waitRisingEdge(5)

    }
  }
}

object LookupTCamStoppableSim {
  import AssignmentFunctions._
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new LookupTCamStoppable(32, 8, false)) { dut =>
      dut.clockDomain.forkStimulus(5)

      dut.io.wr.valid #= false
      dut.io.rd.req.valid #= false
      dut.io.rd.res.ready #= false

      dut.clockDomain.waitRisingEdge(5)
      dut.io.rd.res.ready #= true

      val wrFlow = new FlowDriver(dut.io.wr, dut.clockDomain)
      wrFlow #= SeqDataGen(
        (0x4321, 0xFFF0, 4, true),
        (0x5321, 0xFFF0, 6, true)
      )

      val masterSim = new StreamDriver(dut.io.rd.req, dut.clockDomain)
      masterSim #= SeqDataGen(0x4322, 0x5322, 0x7332, 0x6332, 0x7332)

      dut.clockDomain.waitRisingEdge(5)

    }
  }
}

object LookupTableStoppableSim {

  import AssignmentFunctions._
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new LookupTableStoppable(32, 32, 16)) { dut =>
      dut.clockDomain.forkStimulus(5)

      dut.io.wr.req.valid #= false
      dut.io.wr.shootDown.valid #= false
      dut.io.rd.req.valid #= false
      dut.io.rd.res.ready #= false

      dut.clockDomain.waitRisingEdge(5)

      val wrReq = new StreamDriver(dut.io.wr.req, dut.clockDomain)
      val rdReq = new StreamDriver(dut.io.rd.req, dut.clockDomain)
      val wrShoot = new FlowDriver(dut.io.wr.shootDown, dut.clockDomain)
      dut.io.rd.res.ready #= true

      fork {
        dut.clockDomain.waitRisingEdge(12)
        dut.io.rd.res.ready #= false
        dut.clockDomain.waitRisingEdge(3)
        dut.io.rd.res.ready #= true
      }

      fork {
        dut.clockDomain.waitRisingEdge(2)
        val insertSeq = (0 until 20).map(i => (0x4320 + i, 0xF0FF, 0x1230 + i))
        wrReq #= SeqDataGen(insertSeq : _*)
      }

      val rdThread = fork {
        dut.clockDomain.waitRisingEdge(10)
        rdReq #= SeqDataGen(0x4321, 0x4325, 0x432F)

        wrShoot #= SeqDataGen(5)

        rdReq #= SeqDataGen(0x4321, 0x4325, 0x432F)
      }

      rdThread.join()



      dut.clockDomain.waitRisingEdge(10)
    }
  }
}

object SequencerSim {
  import AssignmentFunctions._
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new Sequencer(64, 40, 128, 32)) { dut =>
      dut.clockDomain.forkStimulus(5)
    }
  }

}

object LockCounterRamSim {
  import AssignmentFunctions._
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new LockCounterRam(128, 128)) { dut =>
      dut.clockDomain.forkStimulus(5)

      val lockDriver = new FlowDriver(dut.io.lockReq, dut.clockDomain)
      val unlockDriver = new FlowDriver(dut.io.unlockReq, dut.clockDomain)
      val popDriver = new FlowDriver(dut.io.popReq, dut.clockDomain)

      dut.clockDomain.waitRisingEdge(5)

      joinAll (
        fork {
          lockDriver #= SeqDataGen(1, 1, 2, 3, 4, 5)
        },
        fork {
          dut.clockDomain.waitRisingEdge(1)
          unlockDriver #= SeqDataGen(1, 1, 2, 3, 4, 5)
        },
        fork {
          dut.clockDomain.waitRisingEdge(10)
          popDriver #= SeqDataGen(2, 5, 4, 1, 3, 1)
        }
      )

    }
  }
}
