package wuklab

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4

import Utils._

class AxiSimpleDMAReadOnly(implicit config : CoreMemConfig) extends Component {
  val lenWidth = widthOf(config.accessAxi4Config.lenType)
  val io = new Bundle {
    val bus = master (Axi4(config.accessAxi4Config))
    val dma = slave (
      AxiStreamDMAInterface(AxiStreamDMAConfig(config.dmaAxisConfig, config.physicalAddrWidth, config.dmaLengthWidth))
    )
  }

  // Let Write free run
  io.dma.write.data.freeRun()
  io.dma.write.cmd.freeRun()
  io.bus.aw.disable
  io.bus.w.disable
  io.bus.b.freeRun()

  // Read DMA
  io.bus.ar.translateFrom (io.dma.read.cmd) { (axi, axis) =>
//    val addr   = UInt(config.addressWidth bits)
//    val id     = if(config.useId)     UInt(config.idWidth bits)   else null
//    val region = if(config.useRegion) Bits(4 bits)                else null
//    val len    = if(config.useLen)    UInt(8 bits)                else null
//    val size   = if(config.useSize)   UInt(3 bits)                else null
//    val burst  = if(config.useBurst)  Bits(2 bits)                else null
//    val lock   = if(config.useLock)   Bits(1 bits)                else null
//    val cache  = if(config.useCache)  Bits(4 bits)                else null
//    val qos    = if(config.useQos)    Bits(4 bits)                else null
//    val user   = if(userWidth >= 0)   Bits(userWidth bits)        else null
//    val prot   = if(config.useProt)   Bits(3 bits)                else null

    axi.addr := axis.addr.resized
    axi.size := Axi4.size.BYTE_64.asUInt
    axi.len := ((axis.len >> 6) - 1).resized
    axi.lock := Axi4.lock.NORMAL
    axi.cache := B"0000"
    axi.prot := B"010"
    axi.setBurstINCR()
  }

  io.dma.read.data.translateFrom (io.bus.r) { (axis, axi) =>
    axis.fragment := axi.data
    axis.last := axi.last
  }
}
