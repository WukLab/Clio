package wuklab.test

import spinal.core._
import spinal.core.sim._
import wuklab._
import wuklab.sim._
import SimContext._
import scodec.bits.ByteVector
import util.Random

object CoreMemSim {
  import SimConversions._
  def main(args: Array[String]): Unit = {
    LegoMemSimConfig.doSim(
      new MemoryAccessEndPoint
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


      val dataStream = new StreamDriver(dut.io.ep.dataIn, dut.clockDomain)
      val ctrlStream = new StreamDriver(dut.io.ep.ctrlIn, dut.clockDomain)
      dut.io.ep.dataOut.ready #= true
      dut.io.ep.ctrlOut.ready #= true

      println("Initializtion finish")
      config.printConfig

      dut.clockDomain.assertReset()
      dut.clockDomain.waitRisingEdge(5)
      dut.clockDomain.deassertReset()

      dut.clockDomain.waitRisingEdge(10)

      def wr_cmd(seq : Int, data : Seq[Byte]) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x50, cont = 0x4321, seqId = seq, size = 28 + data.size),
          0x7e000000, data.size, ByteVector(data))
      ).require
      def rd_cmd(seq : Int, size : Int) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x40, cont = 0x4321, seqId = seq, size = 28),
          0x79000000, size, ByteVector.empty)
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

        for (i <- 1 to 512) {
          val dataBytes = 0x10 + Random.nextInt(256)
          dataStream #= BitAxisDataGen(rd_cmd(i % 256, dataBytes))
          println(f"Read finish $i with $dataBytes bytes")
        }
        dut.clockDomain.waitRisingEdge(30)

        for (i <- 1 to 512) {
          val dataBytes = 0x10 + Random.nextInt(256)
          dataStream #= BitAxisDataGen(wr_cmd(i % 256, (0 until dataBytes).map(_.toByte)))
          println(f"Write finish $i with $dataBytes bytes")
        }
        dut.clockDomain.waitRisingEdge(30)

        for (i <- 1 to 512) {
          val dataBytes = 0x10 + Random.nextInt(256)
          dataStream #= BitAxisDataGen(mig_cmd(i % 256, dataBytes))
          println(f"MIGRATION finish $i with $dataBytes bytes")
        }

      }

      val ctrlThread = fork {
        //        ctrlStream #= BitStreamDataGen(
        //          Seq(ControlRequestSim(epid = 1, addr = 0).serialize)
        //        )
      }

      dataThread.join()
      dut.clockDomain.waitRisingEdge(8000)
    }}

  }
}
