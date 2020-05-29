package wuklab.test

import spinal.core._
import spinal.core.sim._
import wuklab._
import wuklab.sim.{ControlRequestSim, _}
import scodec.bits.ByteVector
import wuklab.kv._

import scala.util.Random

object KeyValueVirtualEndpointSim {
  def main(args: Array[String]): Unit = {
    import SimConversions._
    import KeyValueSimContext.LegoMemSimConfig
    import KeyValueGenerateContext._
    LegoMemSimConfig.doSim(
      new KeyValueVirtualEndpoint
    ) {dut => {
      dut.clockDomain.forkStimulus(5)

      val dataStream = new StreamDriver(dut.io.ep.dataIn, dut.clockDomain)
      val ctrlStream = new StreamDriver(dut.io.ep.ctrlIn, dut.clockDomain)
      dut.io.ep.dataOut.ready #= true
      dut.io.ep.ctrlOut.ready #= true

      println("Initializtion finish, wating reset ...")
      dut.clockDomain.assertReset()
      dut.clockDomain.waitRisingEdge(10)
      dut.clockDomain.deassertReset()
      dut.clockDomain.waitRisingEdge(10)

      println("Initializtion & reset finish ...")
      // We need a LegoMem Simulator here!

      object kvOp {
        val read = 0x60
        val write = 0x62
        val update = 0x64
        val delete = 0x66
      }
      def kv_cmd(opCode : Int, key : BigInt, data : Seq[Int] = Seq(), size : Int = 0) = {
        val fullData = key.toByteArray.reverse.padTo(8, 0.toByte) ++ data.map(_.toByte)
        pprint.pprintln(fullData)
        val dataSize = if (data.size == 0) size else data.size
        keyValueMessageCodes.encode(
          (
            KeyValueHeaderSim(
              LegoMemHeaderSim(pid = 0xdead, tag = 0, reqType = opCode, size = 16 + 4 + fullData.size),
              user = 0, valueSize = data.size
            ),
            ByteVector(fullData)
          )
        ).require
      }
      def rd_repl(seq : Int, data : Seq[Int]) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x41, cont = 0x4321, seqId = seq, size = 28 + data.size),
          0x7e000000, data.size, ByteVector(data.map(_.toByte)))
      ).require

      val dataThread = fork {

        // Wait for ctrl to setup
        dut.clockDomain.waitRisingEdge(30)

        println(f"Start testing, write command")
        val write_size = 100
//        dataStream #= BitAxisDataGen(kv_cmd(kvOp.write, 0x12345678, 0 until write_size))
//        dut.clockDomain.waitRisingEdge(30)
//        dataStream #= BitAxisDataGen(rd_repl(0, Seq.fill(64)(0xFF)))
//        dut.clockDomain.waitRisingEdge(30)
//        // The loopback, next cycle
//        dataStream #= BitAxisDataGen(rd_repl(0, Seq.fill(64)(0)))
//        dut.clockDomain.waitRisingEdge(30)
        val key = BigInt("1234567890", 16)

        println(f"Start testing, read command")
        val read_size = 100
        dataStream #= BitAxisDataGen(kv_cmd(kvOp.write, key, data = 0 until read_size))
        dut.clockDomain.waitRisingEdge(30)

        dataStream #= BitAxisDataGen(rd_repl(0, Seq.fill(64)(0)))
        dut.clockDomain.waitRisingEdge(10)
//
//        dataStream #= BitAxisDataGen(rd_repl(1, Seq.fill(20)(0xac)))
//        dut.clockDomain.waitRisingEdge(30)

      }
      val ctrlThread = fork {
        ctrlStream #= SeqDataGen(
          // Baseaddr
          ControlRequestSim(param32 = 0x3c000000, addr = 0xFF, beats = 0, cmd = 0),
          // Pid
          ControlRequestSim(param32 = 60001, addr = 0xFF, beats = 1, cmd = 1),

          // HTE addr
          ControlRequestSim(param32 = 0x7ccc0000, addr = 0),
          // entry addr
          ControlRequestSim(param32 = 0x00400000, addr = 1),
          ControlRequestSim(param32 = 0x00800000, addr = 1)
        )
      }

      dataThread.join()
      ctrlThread.join()

      dut.clockDomain.waitRisingEdge(200)
    }}

  }
}
