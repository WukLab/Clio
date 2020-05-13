package wuklab.kv

import wuklab._

import spinal.core._
import spinal.lib._

trait Compact[T <: Compact[T]] {
  def compactWidth : Int
  def compress(t : T) : Bits
  def decompress(bits: Bits) : T
}

// This is a object view
trait View[ViewT <: Data, T <: Data] {
  implicit def toView   (t : T) : ViewT
  implicit def fromView (t : ViewT) : T
  implicit def assignToView   (view : ViewT, t : T) : Unit = view := toView(t)
  implicit def assignFromView (t : T, view : ViewT) : Unit = t := fromView(view)
}

trait KeyValueConfig extends LegoMemConfig {
  val blockSizes = Seq(32 Bytes, 64 Bytes, 128 Bytes, 256 Bytes, 512 Bytes)

  // dma config
  def dmaControllerReadConfig = ???
  def cmdControllerWriteConfig = ???

  def physicalAddressWidth = 40

}

// The Keyvalue LegoMem header
object KeyValueHeader extends Compact[KeyValueHeader] {
  override def compactWidth: Int = 32
  override def compress(t: KeyValueHeader): Bits = {
    val bits = Bits(compactWidth bits)
    bits
  }
  override def decompress(bits: Bits): KeyValueHeader = {
    KeyValueHeader()
  }
}
case class KeyValueHeader() extends Bundle with Header[KeyValueHeader] {
  val header = LegoMemHeader()
  val user = UInt(12 bits)
  val keySize = UInt(4 bits)
  val valueSize = UInt(16 bits)

  override val packedWidth = header.packedWidth + 32
  override def getSize = header.getSize
  override def fromWiderBits(bits: Bits) = {
    assignFromBits(bits)
    this
  }

}

