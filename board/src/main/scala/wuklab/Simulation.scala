package wuklab

import spinal.core._
import spinal.sim._
import spinal.core.sim._

import scala.util.Random


//MyTopLevel's testbench
object MyTopLevelSim {
  def main(args: Array[String]) {
    SimConfig.withWave.doSim(new MyTopLevel){dut =>
      //Fork a process to generate the reset and the clock on the dut
      dut.clockDomain.forkStimulus(period = 10)

      var modelState = 0
      for(idx <- 0 to 99){
        //Drive the dut inputs with random values
        dut.io.cond0 #= Random.nextBoolean()
        dut.io.cond1 #= Random.nextBoolean()

        //Wait a rising edge on the clock
        dut.clockDomain.waitRisingEdge()

        //Check that the dut values match with the reference model ones
        val modelFlag = modelState == 0 || dut.io.cond1.toBoolean
        assert(dut.io.state.toInt == modelState)
        assert(dut.io.flag.toBoolean == modelFlag)

        //Update the reference model value
        if(dut.io.cond0.toBoolean) {
          modelState = (modelState + 1) & 0xFF
        }
      }
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
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new LookupTCamStoppable(32, 8, false)) { dut =>
      dut.clockDomain.forkStimulus(5)

      dut.io.wr.valid #= false
      dut.io.rd.req.valid #= false
      dut.io.rd.res.ready #= false


      dut.clockDomain.waitRisingEdge(5)

      dut.io.wr.key   #= 0x4321
      dut.io.wr.mask  #= 0xFFF0
      dut.io.wr.value #= 7
      dut.io.wr.valid #= true

      dut.clockDomain.waitRisingEdge(1)
      dut.io.wr.valid #= false

      dut.io.rd.res.ready #= true
      dut.io.rd.req.valid #= true
      dut.io.rd.req.key   #= 0x4322
      dut.clockDomain.waitRisingEdge(1)
      dut.io.rd.res.ready #= false

      dut.io.rd.req.key   #= 0x4332
      dut.clockDomain.waitRisingEdge(1)
      dut.io.rd.req.valid #= false

      dut.clockDomain.waitRisingEdge(5)

    }
  }
}
