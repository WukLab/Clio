package wuklab.test

import scodec.bits.ByteVector
import spinal.core._
import spinal.core.sim._
import wuklab.monitor._
import wuklab.sim._
import wuklab.test.CoreMemSimContext.{LegoMemSimConfig, config}

import scala.util.Random

object NetworkProtocolCheckerSim {
  import SimConversions._
  def main(args: Array[String]): Unit = {
    LegoMemSimConfig.doSim(
      new NetworkProtocolChecker(BigInt("A000D000", 16))
    ) {dut => {
      dut.clockDomain.forkStimulus(5)

      val dataStream   = new StreamDriver(dut.io.dataIn, dut.clockDomain)
      val headerStream = new StreamDriver(dut.io.headerIn, dut.clockDomain)
      dut.io.dataOut.ready #= true
      dut.io.headerOut.ready #= true

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

      // Start Simulation
      val numPackets = 256
      val headerFlow = fork {
        for (i <- 0 until numPackets) {
          val totalBytes = 0x10 + i + 28
          headerStream #= SeqDataGen(UDPHeaderSim(totalBytes, 1, 2, 3, 4))
          println(f"Header finish $i with $totalBytes bytes")
        }
      }
      val dataFlow = fork {
        var total_data = 0
        for (i <- 0 until numPackets) {
          val dataBytes = 0x10 + i
          dataStream #= BitAxisDataGen(wr_cmd(i % 256, (0 until dataBytes).map(_.toByte)))
          println(f"Data finish $i with $dataBytes bytes")
          total_data += dataBytes + 28
        }
        println(s"Total Data Bytes sent $total_data%X")
      }

      headerFlow.join()
      dataFlow.join()

      dut.clockDomain.waitRisingEdge(80)
    }}

  }
}
