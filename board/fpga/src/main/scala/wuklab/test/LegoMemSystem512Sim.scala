package wuklab.test

import spinal.core._
import spinal.core.sim._
import wuklab._
import wuklab.sim._
import scodec.bits.ByteVector
import spinal.lib.bus.amba4.axi.Axi4
import spinal.lib.{Fragment, master, slave}

import Utils._

import CoreMem512SimContext._
object LegoMemSystem512Sim {
  import SimConversions._
  def main(args: Array[String]): Unit = {
    LegoMemSimConfig.doSim(
      new Component {
        val io = new Bundle {
          val net = NetworkInterface(config.networkDataWidth)
          val bus = new Bundle {
            val access = master (Axi4(config.accessAxi4Config))
            val lookup = master (Axi4(config.lookupAxi4Config))
          }
        }

        val system = new LegoMemSystem
        io.net <> system.io.net
        io.bus.access <> system.io.bus.access
        io.bus.lookup <> system.io.bus.lookup

        system.io.eps.seq.dataOut >> system.io.eps.mem.dataIn
        system.io.eps.seq.dataIn << system.io.eps.mem.dataOut
        system.io.eps.seq.ctrlIn.disable
        system.io.eps.seq.ctrlOut.freeRun()
        system.io.eps.mem.ctrlIn.disable
        system.io.eps.mem.ctrlOut.freeRun()

        system.io.eps.soc.dataIn.disable
        system.io.eps.soc.dataOut.freeRun()
        system.io.eps.soc.ctrlIn.disable
        system.io.eps.soc.ctrlOut.freeRun()
      }
    ) {dut => {
      dut.clockDomain.forkStimulus(5)


      // Setup xbar
      val mem = new DictMemoryDriver(dut.clockDomain)
      mem =# dut.io.bus.access
      mem =# dut.io.bus.lookup
      mem.initValue(
        // Page Table
        (BigInt("52001F800", 16), PageTableEntrySim(
          ppa = BigInt("540000000", 16),
          tag = BigInt("0001000000007E0", 16),
          allocated = true,
          used = true
        ))
      ).init(
        // The data
        (BigInt("540000000", 16), (0 until 256).map (_.toByte))
      )


      val dataStream = new StreamDriver(dut.io.net.dataIn, dut.clockDomain)
//      val headerStream = new StreamDriver(dut.io.net.headerIn, dut.clockDomain)
      dut.io.net.headerIn.valid #= true
      dut.io.net.headerIn.payload #= 0

      dut.io.net.headerOut.ready #= true

      println("Initializtion finish")
      config.printConfig

      dut.clockDomain.assertReset()
      dut.clockDomain.waitRisingEdge(5)
      dut.clockDomain.deassertReset()

      dut.clockDomain.waitRisingEdge(10)

      def rd_cmd(seq : Int, size : Int) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x40, cont = 0x4321, seqId = seq, size = 28),
          0x7e000000, size, ByteVector.empty)
      ).require
      def mig_cmd(seq : Int, size : Int) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x45, cont = 0x4321, seqId = seq, size = 28),
          0x7e000000, size, ByteVector.empty)
      ).require

      // Start Simulation
      val dataThread = fork {
        // Setup the cache
        dataStream #= BitAxisDataGen(mig_cmd(0, 0x10))
        dut.clockDomain.waitRisingEdge(30)

        //        println(f"flush Test Started")
        //        for (i <- 1 to 512) {
        //          val dataBytes = 0x10 + Random.nextInt(256)
        //          dataStream #= BitAxisDataGen(rd_cmd(i % 256, dataBytes))
        //          dut.clockDomain.waitRisingEdge(10)
        //          dataStream #= BitAxisDataGen(flush_cmd(i % 256))
        //          println(f"Flush finish $i with $dataBytes bytes")
        //          dut.clockDomain.waitRisingEdge(10)
        //        }
        //        dut.clockDomain.waitRisingEdge(30)

        println(f"Read Test Started")
        for (i <- 0 until 512) {
          val dataBytes = 1024
          dataStream #= BitAxisDataGen(rd_cmd(i % 256, dataBytes))
          println(f"Read finish $i with $dataBytes bytes")
        }
        dut.clockDomain.waitRisingEdge(30)
        //
        //        println(f"Write Test Started")
        //        for (i <- 1 to 512) {
        //          val dataBytes = 0x10 + Random.nextInt(256)
        //          dataStream #= BitAxisDataGen(wr_cmd(i % 256, (0 until dataBytes).map(_.toByte)))
        //          println(f"Write finish $i with $dataBytes bytes")
        //        }
        //        dut.clockDomain.waitRisingEdge(30)
        //
        //        for (i <- 1 to 512) {
        //          val dataBytes = 0x10 + Random.nextInt(256)
        //          dataStream #= BitAxisDataGen(mig_cmd(i % 256, dataBytes))
        //          println(f"MIGRATION finish $i with $dataBytes bytes")
        //        }

      }

      val headerThread = fork {
          dut.io.net.dataOut.ready #= true
      }

      dataThread.join()
      dut.clockDomain.waitRisingEdge(8000)
    }}

  }
}
