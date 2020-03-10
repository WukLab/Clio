package wuklab

import spinal.core._
import spinal.lib._

import Utils._

// TODO: use shapeless to define conversions
object UDPHeader {
  def compactWidth = 48

}

// TODO: finish this
case class UDPHeader() extends Bundle {
  val length = UInt(12 bits)
  val dest_port = UInt(16 bits)
  val source_port = UInt(16 bits)
  val ip_dest_ip = UInt(32 bits)
  val ip_source_ip = UInt(32 bits)

  def compactWidth = UDPHeader.compactWidth
  def asCompactBits : Bits = {
    val bits = Bits(compactWidth bits)
    bits
  }
  def fromCompactBits(bits : Bits) : Unit = {
    assert(compactWidth == bits.getWidth, "UDPHeader: Compact width mismatch")
  }
}

class NetworkAdapter extends Component {
  val netConfig = AxiStreamConfig(64, keepWidth = 8)

  val io = new Bundle {
    val net = new Bundle {
      val dataIn    = slave Stream Fragment(AxiStreamPayload(netConfig))
      val dataOut   = master Stream Fragment(AxiStreamPayload(netConfig))
      // TODO: check if we need pack
      val headerIn  = slave Stream UDPHeader()
      val headerOut = master Stream UDPHeader()
    }

    val seq = new Bundle {
      val dataIn  = slave Stream Fragment (Bits(512 bits))
      val dataOut = master Stream Fragment (Bits(512 bits))
    }
  }

  // First, we have the width converter for the in and out traffic.
  val numIds = 256
  // Assign Sequence from the ID Pool
  val ids = new IDPool(numIds)
  // Header -> Compact -> Resp
  val mem = Mem(HardType(Bits(UDPHeader.compactWidth bits)), numIds)

  val inputCtrl = new Area {
    // the header and the id allocation relied on the last signal
    val dataIn = io.net.dataIn.continueWhen(ids.io.alloc.valid)

    // Save the header
    val writeHeader = (io.net.headerIn >*< ids.io.alloc).continueWhen(io.net.dataIn.lastFire).asFlow
    mem.writePort.translateFrom (writeHeader) { (cmd, p) =>
      cmd.address := p.snd
      cmd.data := p.fst.asCompactBits
    }
  }

  val outputCtrl = new Area {
    // The control is han over to the data path. the data path should block on ctrl
    val dataIn = io.seq.dataIn.continueWhen(io.net.headerOut.ready)

    // Lookup the header
    val header = LegoMemHeader(dataIn.fragment)
    // Since memory read only takes one cycle, I guess its safe..
    val length = RegNextWhen(header.size, dataIn.firstFire)

    val rdPort = mem.readSyncPort
    rdPort.cmd << ReturnFlow(header.seqId, dataIn.firstFire)

    io.net.headerOut.translateFrom (ReturnStream(rdPort.rsp, RegNext(dataIn.firstFire))) { (header, bits) =>
      header.fromCompactBits(bits)
      // TODO: set source IP, extra length?
      header.length := length
    }


  }

  val widthConverter = new LegoMemDataOutputAdapter(64)
  widthConverter.io.external.dataIn << io.net.dataIn
  widthConverter.io.external.dataOut >> io.net.dataOut
  widthConverter.io.internal.dataIn << outputCtrl.dataIn
  widthConverter.io.internal.dataOut >> io.seq.dataOut

}

