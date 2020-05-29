package wuklab.kv

import wuklab._
import spinal.lib._
import spinal.core._

import Utils._

// Control path
// Alloc, init,
class KeyValueController(implicit config : KeyValueConfig) extends Component {
  val io = new Bundle {
    val ctrl = slave (LegoMemControlEndpoint())
    // Requests
    val regs = new Bundle {
      val baseAddr = out UInt(config.dmaAddrWidth bits)
      val pid      = out UInt(16 bits)
    }
  }

  // EPID for the controller
  io.ctrl.in.freeRun()
  io.ctrl.out.disable

  val baseAddrReg = RegNextWhen(
    io.ctrl.in.resizeParam(config.dmaAddrWidth),
    io.ctrl.in.fire && io.ctrl.in.cmd === U"4'h0"
  )

  val pidReg      = RegNextWhen(
    io.ctrl.in.resizeParam(16),
    io.ctrl.in.fire && io.ctrl.in.cmd === U"4'h1"
  )

  io.regs.baseAddr := baseAddrReg
  io.regs.pid := pidReg
}

//// Good old arbtriting
//class KeyValueDMAController extends Component {
//  val numFullChannels = 4
//  val numReadChannels = 4
//  val numWriteChannels = 4
//
//  val numOutstandingReads = 256
//  // tag: channel ID
//  val io = new Bundle {
//    val fulls   = Vec(slave (AxiStreamDMAInterface()), numReadChannels)
//    val reads   = Vec(slave (AxiStreamDMAReadInterface()), numReadChannels)
//    val writes  = Vec(slave (AxiStreamDMAWriteInterface()), numWriteChannels)
//
//    val dma     = master (AxiStreamDMAInterface())
//  }
//
//  val readChannels = io.fulls.map(_.read) ++ io.reads
//  val writeChannels = io.fulls.map(_.write) ++ io.writes
//
//  // Read path
//  val readCtrl = new Area {
//    io.dma.read.data << StreamArbiterFactory.roundRobin.on(readChannels.map(_.cmd))
//
//    val fireVec = Vec(io.reads.map(_.cmd.fire))
//    val fireFifo = ReturnStream(fireVec, io.dma.read.data.fire).queue(log2Up(numOutstandingReads))
//
//    val readResults = StreamDemuxOH(io.dma.read.data, fireFifo.payload)
//    fireFifo.throwWhen(io.dma.read.data.lastFire)
//    (io.reads, readResults).zipped map (_.data << _)
//  }
//
//  // Write Path
//  val writeCtrl = new Area{
//    io.dma.read.data << StreamArbiterFactory.roundRobin.on(io.writes.map(_.cmd))
//
//    val fireVec = Vec(io.reads.map(_.cmd.fire))
//    val fireFifo = ReturnStream(fireVec, io.dma.read.data.fire).queueLowLatency(2)
//
//    val readResults = StreamDemuxOH(io.dma.read.data <* fireFifo, fireFifo.payload)
//    (io.reads, readResults).zipped map (_.data << _)
//  }
//
//}
