package wuklab.test

import scodec.bits.ByteVector
import spinal.core.sim._
import wuklab._
import wuklab.sim._

object DataFrameEndpointSim {
  import CoreMemSimContext._
  import SimConversions._

  // The struct is
  // {
  //    key : uint64,
  //    next : uint64,
  //    value : uint64 -> 16 bytes
  // }
  def pc_cmd(addr : Long, key : Long, len : Int = 24, useDepth : Boolean = false, depth : Int = 0) = pointerChasingHeaderCodec.encode(
    PointerChasingHeaderSim(
      LegoMemHeaderSim(
        pid = 0xdead, tag = 0, reqType = 0x71, size = 80,
        destIp = 0x70a800a0, destPort = 2333, cont = 0xFF01
      ),
      addr = addr, key = key,
      structSize = len, valueSize = 16,
      keyOffset = 0, valueOffset = 16, depth = depth,
      nextOffset = 1,
      useDepth = useDepth, linkValue = useDepth, useKey = useDepth
    )
  ).require

  def rd_repl(seq : Int, data : ByteVector) = legoMemAccessMsgCodec.encode(
    (LegoMemHeaderSim(pid = 0xdead, tag = 0, reqType = 0x41, cont = 0x4321, seqId = seq, size = 28 + data.size.toInt),
      0x7e000000, data.size.toInt, data)
  ).require

  def section(key : Long, next : Long, value : Long) : ByteVector = {
    def bv(l : Long) = ByteVector(BigInt(l).toByteArray.reverse.padTo(8, 0.toByte))
    (bv(key) ++ bv(next) ++ bv(value))
  }


  def main(args: Array[String]): Unit = {
    LegoMemSimConfig.doSim(
      new DataFrameEndpoint
    ) { dut =>
      dut.clockDomain.forkStimulus(5)

      val dataStream = new StreamDriver(dut.io.dataIn, dut.clockDomain)
      dut.io.ctrlIn.valid #= false
      dut.io.dataOut.ready #= true
      dut.io.ctrlOut.ready #= true

      println("Initializtion finish, wating reset ...")
      dut.clockDomain.assertReset()
      dut.clockDomain.waitRisingEdge(10)
      dut.clockDomain.deassertReset()
      dut.clockDomain.waitRisingEdge(10)

      println("Initializtion & reset finish ...")
      // We need a LegoMem Simulator here!


      val dataThread = fork {
        // Wait for ctrl to setup
        dut.clockDomain.waitRisingEdge(30)

        println(f"Start testing, filter")

        for (i <- 2 until 32) {

          dataStream #= BitAxisDataGen(pc_cmd(0xcc1234, 0, i * 64))
          dut.clockDomain.waitRisingEdge(30)

          dataStream #= BitAxisDataGen(rd_repl(0, ByteVector(
            Array.fill(i * 64 - 64)(0).map(_.toByte) ++ Array.fill(64)(1).map(_.toByte)
          )))
        }

        dut.clockDomain.waitRisingEdge(100)

        println(f"Start testing, Null")

        for (i <- 1 until 16) {
          val data_len = i * 64
          dataStream #= BitAxisDataGen(pc_cmd(0xcc1234, 1, data_len))
          dut.clockDomain.waitRisingEdge(30)

          dataStream #= BitAxisDataGen(rd_repl(0, ByteVector(Array.fill(data_len)(1).map(_.toByte))))
        }

        dut.clockDomain.waitRisingEdge(100)
      }

      dataThread.join()
      dut.clockDomain.waitRisingEdge(200)

    }
  }
}
