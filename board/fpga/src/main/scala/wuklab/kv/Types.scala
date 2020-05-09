package wuklab.kv

import wuklab._

import spinal.core._
import spinal.lib._

trait KeyValueConfig extends LegoMemConfig {
  val blockSizes = Seq(32 Bytes, 64 Bytes, 128 Bytes, 256 Bytes, 512 Bytes)

  // dma config
  def dmaControllerReadConfig = ???
  def cmdControllerWriteConfig = ???

}

// The Keyvalue LegoMem header
case class KeyValueHeader() extends Bundle with Header[KeyValueHeader] {
  val header = LegoMemHeader()
  val user = UInt(8 bits)
  val keySize = UInt(4 bits)
  val valueSize = UInt(12 bits)

  override val packedWidth =
  override def getSize = header.getSize
  override def fromWiderBits(bits: Bits) = {
    assignFromBits(bits)
    this
  }

}

