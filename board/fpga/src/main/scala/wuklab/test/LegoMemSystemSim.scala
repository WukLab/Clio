package wuklab.test

import spinal.core._
import spinal.core.sim._
import wuklab._
import wuklab.sim._
import SimContext._
import scodec.bits.ByteVector

object LegoMemSystemSim {
  import SimConversions._
  def main(args: Array[String]): Unit = {
    LegoMemSimConfig.doSim(
      new LegoMemSystem
    ) {dut => {
      dut.clockDomain.forkStimulus(5)

      // Setup xbar
      val xbar = new LegoMemEndPointInterconnect(dut.clockDomain)
      xbar.=# (0) (dut.io.eps.seq)
      xbar.=# (1) (dut.io.eps.mem)
      xbar.=# (2) (dut.io.eps.soc)

      // Shut the soc ports
      StreamHalt (dut.io.soc.dataIn)
      StreamHalt (dut.io.soc.dataOut)
      StreamHalt (dut.io.soc.ctrlIn)
      val ctrlStream   = new StreamDriver(dut.io.soc.ctrlOut, dut.clockDomain)

      val mem = new DictMemoryDriver(dut.clockDomain)
      mem =# dut.io.bus.access
      mem =# dut.io.bus.lookup
      mem.initValue(
        (BigInt("52001F800", 16), PageTableEntrySim(
          ppa = BigInt("540000000", 16),
          tag = BigInt("0001000000007E0", 16),
          allocated = true,
          used = true
        ))
      )

      val dataStream   = new StreamDriver(dut.io.net.dataIn, dut.clockDomain)
      val headerStream = new StreamDriver(dut.io.net.headerIn, dut.clockDomain)

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
      def rd_cmd(seq : Int) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x40, cont = 0x4321, seqId = seq, size = 28),
          0x7e000000, 16, ByteVector.empty)
      ).require

      // Start Simulation
      fork {
        headerStream #= SeqDataGen(
          UDPHeaderSim(28 + 65, 0xcaca, 0xdbdb, 0, 1)
        )
      }
      fork {
//        dataStream #= BitAxisDataGen(wr_cmd(0, Array.fill(65)(0x73 : Byte)))
        dataStream #= BitAxisDataGen(rd_cmd(1))
      }
      fork {
//        ctrlStream #= BitStreamDataGen(
//          Seq(ControlRequestSim(epid = 1, addr = 0).serialize)
//        )
      }

      dut.clockDomain.waitRisingEdge(80)
    }}

  }
}
