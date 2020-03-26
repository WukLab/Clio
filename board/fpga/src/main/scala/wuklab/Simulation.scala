package wuklab

import scodec.bits.ByteVector
import wuklab.sim.{AddressLookupRequestSim, _}
import spinal.core._
import spinal.core.sim._
import spinal.lib.bus.amba4.axi.Axi4
import spinal.lib.{Fragment, master, slave}
import wuklab.PageFaultUnitSim.FullPageFault

case class CoreMemSimConfig() extends CoreMemConfig {
  val physicalAddrWidth = 16
  val virtualAddrWidth = 64
  val hashtableAddrWidth = 16
  val tagWidth = 40
  val ppaWidth = 16
  val pageSizes = Seq[Int](2,4,8)
  // Cache config
  val numCacheCells = 128
  val numPageFaultCacheCells = 16
  // Hash Table Config
  val hashtableBaseAddr = BigInt(0)
  val pteAddrWidth = 4

  val ptePerLine = 4
  val ptePerBucket = 16
}

object SimContext {
  implicit val config : CoreMemConfig = CoreMemSimConfig()
}
import SimContext._

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
  import AssignmentFunctions._
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.doSim(new FetchUnit) { dut =>

      dut.clockDomain.forkStimulus(5)

      dut.io.res.ready #= false
      val bram = new Axi4SlaveMemoryDriver(dut.clockDomain, 65536) init (
        (0, Seq[Byte](0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16) )
      )
      bram =# dut.io.bus

      val wrReq = new StreamDriver(dut.io.req, dut.clockDomain)

      dut.clockDomain.waitRisingEdge(5)

      dut.io.res.ready #= true

      wrReq #= SeqDataGen(
        (0, 0, 0),
        (0, 0, 0)
      )

      dut.clockDomain.waitRisingEdge(20)

    }
  }
}

object SimulationSpinalConfig extends SpinalConfig(
  defaultConfigForClockDomains = ClockDomainConfig(
    resetActiveLevel = LOW
  )
)

object BlackBoxSim {
  class adder extends BlackBox {
    val io = new Bundle {
      val clk = in Bool
      val rst = in Bool
//      val a = in UInt(4 bits)
      val b = in UInt(4 bits)
      val c = out UInt(4 bits)
    }

    mapCurrentClockDomain(clock = io.clk, reset = io.rst)
    noIoPrefix()
    addRTLPath("src/lib/verilog/adder.v")
  }
  def main(args: Array[String]): Unit = {
    SimConfig
      .withConfig(SimulationSpinalConfig)
      .addSimulatorFlag("-Wno-PINMISSING")
      .addSimulatorFlag("-Wno-CASEINCOMPLETE")
      .withWave.doSim(
      new Component {
        val dma = new axi_dma(config.dmaAxisConfig, config.accessAxi4Config, config.physicalAddrWidth, config.dmaLengthWidth)
        val io = new Bundle {
          val s_axis_read_desc  = slave Stream AxiStreamDMAReadCommand(config.dmaAxisConfig, config.physicalAddrWidth, config.dmaLengthWidth)
          val s_axis_write_desc = slave Stream AxiStreamDMAWriteCommand(config.physicalAddrWidth, config.dmaLengthWidth)
          val m_axis_read_data  = master Stream Fragment(AxiStreamPayload(config.dmaAxisConfig))
          val s_axis_write_data = slave Stream Fragment(AxiStreamPayload(config.dmaAxisConfig))
          // TODO: We ignore the datas
          // val m_axis_write_desc_status_id = master Flow NoData
          // val m_axis_write_desc_status_id = master Flow NoData
          val m_axi = master (Axi4(config.accessAxi4Config))
        }
        dma.io.s_axis_read_desc <> io.s_axis_read_desc
        dma.io.m_axis_read_data <> io.m_axis_read_data
        dma.io.s_axis_write_desc <> io.s_axis_write_desc
        dma.io.s_axis_write_data <> io.s_axis_write_data
        dma.io.m_axi <> io.m_axi
        dma.io.read_enable := True
        dma.io.write_enable := True
        dma.io.write_abort := False
      }
    ) { dut =>

      dut.clockDomain.forkStimulus(5)

    }

  }
}

object CoreMemorySim {
  import SimConversions._

  def main(args: Array[String]): Unit = {
    SimConfig
      .withConfig(SimulationSpinalConfig)
      .addSimulatorFlag("-Wno-PINMISSING")
      .addSimulatorFlag("-Wno-CASEINCOMPLETE")
      .withWave.doSim(
      new MemoryAccessEndPoint
    ) {dut => {
      dut.clockDomain.forkStimulus(5)

      val mem = new Axi4SlaveMemoryDriver(dut.clockDomain, 65536)
      mem =# dut.io.bus.access
      mem =# dut.io.bus.lookup
      val data = new StreamDriver(dut.io.ep.dataIn, dut.clockDomain)
      val ctrl = new StreamDriver(dut.io.ep.ctrlIn, dut.clockDomain)
      println("Initializtion finish")

      dut.clockDomain.assertReset()
      dut.clockDomain.waitRisingEdge(5)
      dut.clockDomain.deassertReset()

      dut.clockDomain.waitRisingEdge(10)

      val dec = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0, tag = 0, reqType = 0, cont = 0, 0, seqId = 0, size = 8), 0x0000L, ByteVector.empty)
      ).require
      data #= BitAxisDataGen(Seq(dec), Seq(dec))

      dut.clockDomain.waitRisingEdge(80)
    }}
  }
}

object PageFaultUnitSim {
  import AssignmentFunctions._
  class FullPageFault extends Component {
    val ppaWidth = 16
    val tagWidth = 48
    val numPageSizes = 3
    val fault = new PageFaultUint()(CoreMemSimConfig())
    val writer = new PageTableWriter()(CoreMemSimConfig())
    fault.io.memWriteData >> writer.io.reqData
    fault.io.memWriteUser >> writer.io.reqUser
    fault.io.memWriteRes << writer.io.res

    fault.io.simPublic()
    writer.io.simPublic()
  }

  def main(args: Array[String]): Unit = {
    SimConfig.withConfig(SimulationSpinalConfig).withWave.doSim(
      new FullPageFault
    ) { dut =>
      dut.clockDomain.forkStimulus(5)
      // We have to add this line to bring the assignment up for submodules
      dut.clockDomain.waitSampling

      dut.fault.io.res.ready #= true
      val req = new StreamDriver(dut.fault.io.req, dut.clockDomain)
      val fifos = dut.fault.io.addrFifos.map(f => new StreamDriver(f, dut.clockDomain))
      val mem = new Axi4SlaveMemoryDriver(dut.clockDomain, 65536)
      mem =# dut.writer.io.bus

      dut.clockDomain.waitRisingEdge(5)

      for ((fifo, idx) <- fifos.zipWithIndex) {
        fork {
          val seq = (1 to 5).map(_ + 0x10 * idx)
          fifo #= SeqDataGen(seq: _*)
        }
      }

      req #= SeqDataGen(
        (1, 2, true, true, 3),
        (4, 5, true, false, 6)
      )

      dut.clockDomain.waitRisingEdge(20)

    }
  }
}

object AddressLookupUnitSim {
  import AssignmentFunctions._
  import SimConversions._

  def main(args: Array[String]): Unit = {
    SimConfig.withConfig(SimulationSpinalConfig).withWave.doSim (
      new AddressLookupUnit
    ) { dut =>
      dut.clockDomain.forkStimulus(5)

      dut.io.res.ready #= true
      val req = new StreamDriver(dut.io.req, dut.clockDomain)
      val ctrl = new StreamDriver(dut.io.ctrl.in, dut.clockDomain)
      val mem = new Axi4SlaveMemoryDriver(dut.clockDomain, 65536)
      val ptes = Seq(
        (0x200, PageTableEntrySim(ppa = 0x1234, tag = 0x12, pageType = 0, used = true, allocated = true)),
        (0x400, PageTableEntrySim(ppa = 0x5678, tag = 0x14, pageType = 1, used = true, allocated = true)),
        (0x800, PageTableEntrySim(ppa = 0x9985, tag = 0x18, pageType = 2, used = false, allocated = true))
      )
      mem.init(ptes : _*)
      mem =# dut.io.bus

      dut.clockDomain.waitRisingEdge(5)

      val ctrlMsgs = (0 until 3).map (i => (i, 0, 0x00F0 + i))
      ctrl #= SeqDataGen(ctrlMsgs : _*)

      req #= SeqDataGen(
        AddressLookupRequestSim(seqId = 0, tag = 0x12, reqType = 0),
        AddressLookupRequestSim(seqId = 1, tag = 0x14, reqType = 0),
        AddressLookupRequestSim(seqId = 2, tag = 0x18, reqType = 0),
        AddressLookupRequestSim(seqId = 3, tag = 0x22, reqType = 0)
      )

      dut.clockDomain.waitRisingEdge(80)

      req #= SeqDataGen(
        AddressLookupRequestSim(seqId = 4, tag = 0x13, reqType = 0),
        AddressLookupRequestSim(seqId = 5, tag = 0x17, reqType = 0),
        AddressLookupRequestSim(seqId = 6, tag = 0x1F, reqType = 0),
        AddressLookupRequestSim(seqId = 7, tag = 0x22, reqType = 0)
      )

      dut.clockDomain.waitRisingEdge(80)

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
    SimConfig.withWave.doSim(
      new LookupTableStoppable(32, 32, 16,
        useUsed = true, autoUpdate = true, useHitReport = true)
    ) { dut =>
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
        dut.clockDomain.waitRisingEdge(2)
        dut.io.rd.res.ready #= true
        dut.clockDomain.waitRisingEdge(2)
        dut.io.rd.res.ready #= false
        dut.clockDomain.waitRisingEdge(5)
        dut.io.rd.res.ready #= true
      }

      fork {
        dut.clockDomain.waitRisingEdge(2)
        val insertSeq = (0 until 30).map(i => (0x4320 + i, 0xF0FF, 0x1230 + i, true, i % 16))
        wrReq #= SeqDataGen(insertSeq : _*)
      }

      val rdThread = fork {
        dut.clockDomain.waitRisingEdge(10)
        rdReq #= SeqDataGen(0x4321, 0x4325, 0x432F)

        wrShoot #= SeqDataGen(5, 6)

        rdReq #= SeqDataGen(0x4321, 0x4325, 0x432F)
      }

      rdThread.join()



      dut.clockDomain.waitRisingEdge(10)
    }
  }
}

//object SequencerSim {
//  import AssignmentFunctions._
//  def main(args: Array[String]): Unit = {
//    SimConfig.withWave.doSim(new Sequencer(64, 40, 128, 32)) { dut =>
//      dut.clockDomain.forkStimulus(5)
//    }
//  }
//
//}

object LockCounterRamSim {
  import AssignmentFunctions._
  def main(args: Array[String]): Unit = {
    SimConfig.withWave.addSimulatorFlag("-Wno").doSim(new LockCounterRam(128, 128)) { dut =>
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
