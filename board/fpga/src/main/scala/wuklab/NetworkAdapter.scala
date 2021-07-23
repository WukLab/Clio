package wuklab

import spinal.core._
import spinal.lib._
import Utils._

// TODO: use shapeless to define conversions
object UDPHeader {
  def packedWidth = 112

}

trait BitsInterface {
  def packedWidth : Int
  def asPackedBits : Bits
  def fromPackedBits(bits : Bits) : Unit
}

case class ControllerIO() extends Bundle with IMasterSlave {
  val in  = Stream (ControlRequest())
  val out = Stream (ControlRequest())

  override def asMaster(): Unit = {
    master (in)
    slave  (out)
  }
}

// TODO: finish this
case class UDPHeader() extends Bundle {
  val length        = UInt(16 bits)
  val dest_port     = UInt(16 bits)
  val source_port   = UInt(16 bits)
  val ip_dest_ip    = UInt(32 bits)
  val ip_source_ip  = UInt(32 bits)

  def packedWidth = UDPHeader.packedWidth
  def asPackedBits : Bits = {
    val bits = Bits(packedWidth bits)
    bits(96, 16 bits) := length       .asBits
    bits(80, 16 bits) := dest_port    .asBits
    bits(64, 16 bits) := source_port  .asBits
    bits(32, 32 bits) := ip_dest_ip   .asBits
    bits(0,  32 bits) := ip_source_ip .asBits

    bits
  }
  def fromPackedBits(bits : Bits) : UDPHeader = {
    assert(packedWidth == bits.getWidth, "UDPHeader: Compact width mismatch")
    length        := bits(96, 16 bits).asUInt
    dest_port     := bits(80, 16 bits).asUInt
    source_port   := bits(64, 16 bits).asUInt
    ip_dest_ip    := bits(32, 32 bits).asUInt
    ip_source_ip  := bits(0,  32 bits).asUInt
    this
  }
}

case class NetworkInterface(networkWidth : Int) extends Bundle {
  val netConfig = AxiStreamConfig(networkWidth, keepWidth = networkWidth / 8)

  val dataIn    = slave Stream Fragment(AxiStreamPayload(netConfig))
  val dataOut   = master Stream Fragment(AxiStreamPayload(netConfig))
  // TODO: check if we need pack
  val headerIn  = slave Stream Bits(UDPHeader.packedWidth bits)
  val headerOut = master Stream Bits(UDPHeader.packedWidth bits)

  object parsed {
    def udpHeaderIn = headerIn.fmap(UDPHeader().fromPackedBits(_))
    def udpHeaderOut = {
      val in = Stream(UDPHeader())
      in.fmap(_.asPackedBits) >> headerOut
      in
    }
  }
}

//class NetworkStorageAdapter extends Component {
//
//  val io = new Bundle {
//    val net = NetworkInterface()
//
//    val seq = new Bundle {
//      val dataIn  = slave Stream Fragment (Bits(512 bits))
//      val dataOut = master Stream Fragment (Bits(512 bits))
//    }
//  }
//
//  // First, we have the width converter for the in and out traffic.
//  val numIds = 256
//  // Assign Sequence from the ID Pool
//  val ids = new IDPool(numIds)
//  // Header -> Compact -> Resp
//  val mem = Mem(HardType(Bits(UDPHeader.packedWidth bits)), numIds)
//
//  val inputCtrl = new Area {
//    // the header and the id allocation relied on the last signal
//    val dataIn = io.net.dataIn.continueWhen(ids.io.alloc.valid)
//
//    // Save the header
//    val writeHeader = (io.net.headerIn >*< ids.io.alloc).continueWhen(io.net.dataIn.lastFire).asFlow
//    mem.writePort.translateFrom (writeHeader) { (cmd, p) =>
//      cmd.address := p.snd
//      cmd.data := p.fst
//    }
//  }
//
//  val outputCtrl = new Area {
//    // The control is han over to the data path. the data path should block on ctrl
//    val dataIn = io.seq.dataIn.continueWhen(io.net.headerOut.ready)
//
//    // Lookup the header
//    val header = LegoMemHeader(dataIn.fragment)
//    // Since memory read only takes one cycle, I guess its safe..
//    val length = RegNextWhen(header.size, dataIn.firstFire)
//
//    val rdPort = mem.readSyncPort
//    rdPort.cmd << ReturnFlow(header.seqId, dataIn.firstFire)
//
//    io.net.headerOut.translateFrom (ReturnStream(rdPort.rsp, RegNext(dataIn.firstFire))) { (header, bits) =>
//      // TODO: set source IP, extra length?
////      header.length := length
//    }
//
//
//  }
//
//  val widthConverter = new LegoMemDataOutputAdapter(64)
//  widthConverter.io.external.dataIn << io.net.dataIn
//  widthConverter.io.external.dataOut >> io.net.dataOut
//  widthConverter.io.internal.dataIn << outputCtrl.dataIn
//  widthConverter.io.internal.dataOut >> io.seq.dataOut
//
//}

class NetworkAdapter(implicit config : LegoMemConfig) extends Component {

  // Will record the incoming IP and do the trick
  val currentIp   = U"32'h0"

  val io = new Bundle {
    val net = NetworkInterface(config.networkDataWidth)

    val seq = new Bundle {
      val dataIn  = slave Stream Fragment (Bits(512 bits))
      val dataOut = master Stream Fragment (Bits(512 bits))
    }
  }

  val outputCtrl = new Area {
    // The control is han over to the data path. the data path should block on ctrl
    val (data, header) = StreamFork2(io.seq.dataIn)

    // Lookup the header
    io.net.parsed.udpHeaderOut.translateFrom (header.takeWhen(header.first)) { (header, bits) =>
      val memHeader = LegoMemHeader(bits.fragment)
      header.length := memHeader.size
      header.ip_dest_ip := memHeader.destIp
      header.dest_port := memHeader.destPort.resize(16)
      header.ip_source_ip := currentIp
      header.source_port := memHeader.srcPort.resize(16)
    }

  }

  val widthConverter = new LegoMemDataOutputAdapter(config.networkDataWidth)
  widthConverter.io.external.dataIn << io.net.dataIn
  widthConverter.io.external.dataOut >> io.net.dataOut
  widthConverter.io.internal.dataIn << outputCtrl.data

  val inputCtrl = new Area {
    val headerStream = io.net.parsed.udpHeaderIn.queueLowLatency(16)
    // TODO: header stream only on first
    val dataStreamIn = widthConverter.io.internal.dataOut

    io.seq.dataOut.last := widthConverter.io.internal.dataOut.last
    val replacedFirst = LegoMemHeader.assignToBitsOperation(header => {
      header.destIp := headerStream.ip_source_ip
      // header.destPort := headerStream.source_port(7 downto 0)
      // header.srcPort := headerStream.dest_port(7 downto 0)
    })(dataStreamIn.fragment)
    io.seq.dataOut.fragment := Mux(dataStreamIn.first, replacedFirst, dataStreamIn.fragment)
    io.seq.dataOut.valid := Mux(dataStreamIn.first, headerStream.valid && dataStreamIn.valid, dataStreamIn.valid)

    // TODO: check this
    headerStream.ready := dataStreamIn.first && dataStreamIn.valid && io.seq.dataOut.ready
    dataStreamIn.ready := Mux(dataStreamIn.first, headerStream.valid && io.seq.dataOut.ready, io.seq.dataOut.ready)

    when (headerStream.fire) {
      currentIp := headerStream.ip_dest_ip
    }
  }


}

class NetworkController extends Component {
  val io = new Bundle {
    val ctrl    = slave Stream ControlRequest()
    val session = master Stream UInt(16 bits)
  }

  io.session << io.ctrl.fmap(_.param32(15 downto 0))
}
