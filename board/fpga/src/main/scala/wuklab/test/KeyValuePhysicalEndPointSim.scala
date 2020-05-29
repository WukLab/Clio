package wuklab.test

import spinal.core._
import spinal.core.sim._
import wuklab._
import wuklab.sim.{ControlRequestSim, _}
import scodec.bits.ByteVector
import wuklab.kv._

import scala.util.Random

object KeyValuePhysicalEndPointSim {
  def main(args: Array[String]): Unit = {
    import SimConversions._
    import KeyValueSimContext._
    LegoMemSimConfig.doSim(
      new KeyValuePhysicalEndpoint
    ) {dut => {
      dut.clockDomain.forkStimulus(5)

      val mem = new ArrayMemoryDriver(dut.clockDomain, 65536)
      mem =# dut.io.access

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
      def kv_cmd(opCode : Int, key : BigInt, user : Int = 0, data : Seq[Int] = Seq(), size : Int = 0) = {
        val fullData = key.toByteArray.reverse.padTo(8, 0.toByte) ++ data.map(_.toByte)
        val dataSize = if (data.size == 0) size else data.size
        keyValueMessageCodes.encode(
          (
            KeyValueHeaderSim(
              LegoMemHeaderSim(pid = 0xdead, tag = 0, reqType = opCode, size = 16 + 4 + fullData.size),
              user = user, valueSize = dataSize
            ),
            ByteVector(fullData)
          )
        ).require
      }
      def rd_repl(seq : Int, data : Seq[Int]) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x41, cont = 0x4321, seqId = seq, size = 28 + data.size),
          0x7e000000, data.size, ByteVector(data.map(_.toByte)))
      ).require
      def wr_repl(seq : Int, size : Int) = legoMemAccessMsgCodec.encode(
        (LegoMemHeaderSim(pid = 0x0001, tag = 0, reqType = 0x40, cont = 0x4321, seqId = seq, size = 28),
          0x7e000000, size, ByteVector.empty)
      ).require


      val num_runs = 64
      val num_writes = num_runs
      val num_reads = num_runs
      val num_updates = num_runs
      val key_base = 0xceda6a00

      def runMultiEmitTest() : Unit = {
        println(f"Start testing, write multi commit")
        for (i <- 0 until num_writes) {
          val write_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.write, key_base + i * 0x100, user = i, data = 0 until write_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Write $i\n\n")
        }

        dut.clockDomain.waitRisingEdge(300)

        println(f"** Start testing, read multi commit")
        for (i <- 0 until num_reads) {
          val read_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.read, key_base + i * 0x100, user = i, size = read_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Read $i\n\n")
        }
      }

      def runNormalTest(): Unit = {
        println(f"Start testing, write command")
        for (i <- 0 until num_writes) {
          val write_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.write, key_base + i, user = i, data = 0 until write_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Write $i\n\n")
        }

        dut.clockDomain.waitRisingEdge(300)

        println(f"** Start testing, read command, all hit")
        for (i <- 0 until num_reads) {
          val read_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.read, key_base + i, user = i, size = read_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Read Hit $i\n\n")
        }

        println(f"** Start testing, read command, all miss")
        for (i <- 0 until num_reads) {
          val read_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.read, key_base + 0x100000 + i, user = i, size = read_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Read Miss $i\n\n")
        }

        println(f"** Start testing, update command")
        for (i <- 0 until num_updates) {
          val updateSize = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.update, key_base + i, user = i, data = updateSize downto 0))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Update $i\n\n")
        }

        println(f"** Start testing, read command, all hit")
        for (i <- 0 until num_reads) {
          val read_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.read, key_base + i, user = i, size = read_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Read Updated $i\n\n")
        }

        println(f"** Start testing, read burst")
        for (i <- 0 until num_reads) {
          val read_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.read, key_base + i, user = i, size = read_size))
          dut.clockDomain.waitRisingEdge(10)
        }
        println(f"Finish Read Burst")

      }

      def runCaseTest() : Unit = {
        println(f"Start testing, write command")
        for (i <- 0 until num_writes) {
          val write_size = i + 16
          dataStream #= BitAxisDataGen(kv_cmd(kvOp.write, key_base + i, user = i, data = 0 until write_size))
          dut.clockDomain.waitRisingEdge(100)
          println(f"Finish Write $i\n\n")
        }
      }

      val dataThread = fork {
        runNormalTest()
      }


      // send in the addresses
      val ctrlThread = fork {
        ctrlStream #= SeqDataGen(
          // Base addr
          ControlRequestSim(param8 = 0x0, addr = 0xFF, beats = 0),
          // ProcessID
          ControlRequestSim(param32 = 0xdead, addr = 0xFF, beats = 1),
          // new HTE Address
          // FIFO is big enough for this ones
          ControlRequestSim(param32 = 0x1040, addr = 0),
          ControlRequestSim(param32 = 0x1080, addr = 0),
          ControlRequestSim(param32 = 0x10c0, addr = 0),
          ControlRequestSim(param32 = 0x1100, addr = 0),
          ControlRequestSim(param32 = 0x1140, addr = 0),
          ControlRequestSim(param32 = 0x1180, addr = 0),
          ControlRequestSim(param32 = 0x11c0, addr = 0),
          ControlRequestSim(param32 = 0x1200, addr = 0),
          ControlRequestSim(param32 = 0x1240, addr = 0),
          ControlRequestSim(param32 = 0x1280, addr = 0),
          ControlRequestSim(param32 = 0x12c0, addr = 0),
          ControlRequestSim(param32 = 0x1300, addr = 0)
          // new PTE Address
        )

        for (i <- 0 until num_writes) {
          ctrlStream #= SeqDataGen(ControlRequestSim(param32 = 0x00004000 + (i << 8), addr = 1))
        }
      }

      dataThread.join()
      ctrlThread.join()

      dut.clockDomain.waitRisingEdge(300)

    }}

  }
}
