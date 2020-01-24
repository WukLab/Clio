package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Ar, Axi4Aw, Axi4B, Axi4Config, Axi4R}

// Can use different bus!
class LegoCmd extends Bundle {
  object CmdType extends SpinalEnum {
    val reqReq, user = newElement()
  }

  val rdReq      = Bool
  val contPort   = Vec(UInt(4 bits), 4)
  val contOffset = Vec(UInt(4 bits), 4)
  val size       = UInt(16 bits)
  val params     = Vec(UInt(64 bits), 3)
  val data       = UInt(256 bits)

  def contAddr : UInt = {
    // TODO: this magic numbers works for address like A040 A080 ...
    contPort(0) << 12 | contOffset(0) << 6
  }
}

case class LegoMemBus(val config : Axi4Config) extends Bundle {
  val in  = slave  (Axi4(config))
  val out = master (Axi4(config))
}

trait LegoMemBusSlot {
  val address : Int
}

class WriteFifoSlot(val address : Int) extends LegoMemBusSlot

class CmdWriteSlot[T <: Data](val address : Int, val stream : Stream[T]) extends LegoMemBusSlot

class Axi4ChannelFactory(bus : LegoMemBus) extends Area {

  val coreDataStream = Stream(Bits(512 bits))
  val coreOutstandingCmdStream = Stream(new LegoCmd)
  val coreCmdOutStream = Stream(new LegoCmd)

  val slotWidth = bus.config.addressWidth / 8
  val dataSlots : Seq[LegoMemBusSlot] = Seq.empty
  val cmdSlots : Seq[CmdWriteSlot[_ <: Data]] = Seq.empty
  val dataFifos : Seq[Stream[UInt]] = Seq.empty

  def addDataInterface[T <: Data](slot : Int, stream : Stream[T]): Unit = {
    // Do checks
    assert(stream.isMasterInterface)
    assert(stream.payloadType.getBitsWidth <= bus.config.dataWidth)

    //

    // Add to slots
    cmdSlots :+ new CmdWriteSlot(slot * slotWidth, stream)
  }

  // The write path
  // Write Slave : Aw, B, W
  val (writeThunk, writeBranch) = bus.in.writeCmd.branch()
  val writeJoinEvent = StreamJoin.arg(writeBranch,bus.in.writeData)
  val writeRsp = Axi4B(bus.config)
  writeRsp.setOKAY()
  bus.in.writeRsp <-< writeJoinEvent.translateWith(writeRsp)

  val writeSlotSelect = OHToUInt(Vec(cmdSlots.map(_.address === bus.in.writeCmd.addr)))
  val demuxedWrites = StreamDemux(bus.in.writeData, writeSlotSelect, cmdSlots.size)
  (demuxedWrites, cmdSlots.map(_.stream)).zipped map { (output, fifo) =>
    output.translateInto(fifo)((d, w) => d := w.data.toDataType(d))
  }

  // TODO: connect to cmd stream
  // TODO: Change to multiple Cmd
  val Seq(rdCmd, otherCmd) = StreamDemux(coreOutstandingCmdStream, coreOutstandingCmdStream.rdReq.asUInt, 2)

  // Write master
  // TODO: Change to multiple Cmd
  bus.out.writeCmd <-< coreCmdOutStream.fmap(cmd => {
    val writeReq = new Axi4Aw(bus.config)
    writeReq
  })
//  bus.out.writeData <-< coreCmdOutStream.fmap(_.asBits)
  // TODO: add Status Check
  bus.out.writeRsp.freeRun()


  // The read path
  // Read Slave, read Ar & A
  // TODO: Add a counter here
  val readDataStage = bus.in.readCmd.stage()
  val readData = StreamMux(addrToSlot(readDataStage.addr), dataFifos) fmap (data => {
    val readRsp = Axi4R(bus.config)
    readRsp.setOKAY()
    readRsp.data := data.asBits
    readRsp
  })

  bus.in.readRsp <-< (readDataStage *> readData)

  // Read Master
  bus.out.readCmd << rdCmd.fmap(cmd => buildReadRequest(cmd.size, cmd.contAddr))
  // TODO: add read Status Check
  bus.out.readRsp.fmap(_.data) >-> coreDataStream

  def buildReadRequest(burstLength : UInt, addr : UInt): Axi4Ar = {
    val readReq = Axi4Ar(bus.config)
    readReq.setBurstFIXED()

    readReq
  }

  def addrToSlot(addr : UInt) : UInt = {
    (addr >> 6) & 0xF
  }


}

case class LegoMemConfig(name : Int = 0,
                         numCmdIn : Int = 1,
                         numCmdOut : Int = 1 ) {

}

case class LegoMemComponentInterface(config : LegoMemConfig) extends Bundle with IMasterSlave {
  val cmdIn   = Stream (new LegoCmd)
  val cmdOut  = Stream (new LegoCmd)
  val dataIn  = Stream (UInt(512 bits))
  val dataOut = Stream (UInt(512 bits))

  override def asMaster(): Unit = {
    master (cmdOut)
    master (dataOut)
    slave (cmdIn)
    slave (dataIn)
  }
}

trait Selection[T <: Data] {
  def select(t : T) : UInt
}

