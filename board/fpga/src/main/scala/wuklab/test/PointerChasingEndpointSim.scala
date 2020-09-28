package wuklab.test

import spinal.core._
import spinal.core.sim._
import wuklab._
import wuklab.sim._
import scodec.bits.ByteVector

import scala.util.Random

object PointerChasingEndpointSim {
  import SimConversions._
  import CoreMemSimContext._

  // The struct is
  // {
  //    key : uint64,
  //    next : uint64,
  //    value : uint64 -> 16 bytes
  // }
  def pc_cmd(addr : Long, key : Long, useDepth : Boolean = false, depth : Int = 0) = pointerChasingHeaderCodec.encode(
    PointerChasingHeaderSim(
      LegoMemHeaderSim(
        pid = 0xdead, tag = 0, reqType = 0x71, size = 80,
        destIp = 0x70a800a0, destPort = 2333, cont = 0xFF01
      ),
      addr = addr, key = key,
      structSize = 24, valueSize = 16,
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
      new PointerChasingEndpoint
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

        println(f"Start testing, normal")

        {
          val iter = 16
          dataStream #= BitAxisDataGen(pc_cmd(0xcc1234, 0x1ab+iter-1))
          dut.clockDomain.waitRisingEdge(30)

          for (i <- 0 until iter)
            dataStream #= BitAxisDataGen(rd_repl(0, section(0x1ab + i, 0x2cd + i, 0x3ef + i)))
        }
        dut.clockDomain.waitRisingEdge(100)

        println(f"Start testing, depth")

        {
          val iter = 16
          dataStream #= BitAxisDataGen(pc_cmd(0xcc1234, 0x1ab+iter, true, depth = iter))
          dut.clockDomain.waitRisingEdge(30)

          for (i <- 0 until iter)
            dataStream #= BitAxisDataGen(rd_repl(0, section(0x1ab + i, 0x2cd + i, 0x3ef + i)))
        }
        dut.clockDomain.waitRisingEdge(100)
      }

      dataThread.join()
      dut.clockDomain.waitRisingEdge(200)

    }
  }
}
