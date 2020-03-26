package wuklab.sim

import spinal.core.sim._
import wuklab._
import scodec._
import scodec.bits._
import scodec.codecs._
import spinal.lib.Fragment

import scala.collection.mutable.ArrayBuffer

trait MemSimStruct {
  def asBytes : Seq[Byte]
}

case class PageTableEntrySim(
                              ppa : BigInt,
                              tag : BigInt = 0,
                              pageType : Byte = 0,
                              used : Boolean = false,
                              allocated : Boolean = false
                            ) extends MemSimStruct {
  override def asBytes = SimConversions.pageTableEntrySimToBytes(this)
}

case class AddressLookupRequestSim(
                             seqId : Int,
                             tag : BigInt = 0,
                             reqType : Int = 0
                            ) extends MemSimStruct {
  override def asBytes = SimConversions.LookupRequestSimToBytes(this)
}

//val pid       = UInt(16 bits)
//val tag       = UInt(8 bits)
//val reqType   = UInt(8 bits)
//val cont      = UInt(8 bits)
//val reqStatus = UInt(4 bits)
//val seqId     = UInt(4 bits)
//val size      = UInt(16 bits)

case class LegoMemHeaderSim(
                              pid : Int,
                              tag : Int,
                              reqType : Int,
                              cont : Int,
                              reqStatus : Int,
                              seqId : Int,
                              size : Int
                            )
case class LegoMemHeaderDataSim (header : LegoMemHeaderSim, data : ByteVector = ByteVector.empty)

object SimConversions {
  def assign (field : BigInt, bytes : ArrayBuffer[Byte], offset : Int) : Unit = {
    for ((b, idx) <- field.toByteArray.reverse.zipWithIndex)
      bytes(idx + offset) = b
  }
  def assign (field : Int, bytes : ArrayBuffer[Byte], offset : Int) : Unit = assign(BigInt(field), bytes, offset)

  implicit def pageTableEntrySimToBytes(pte : PageTableEntrySim) : Seq[Byte] = {
    val pteBytes = 16
    val bytes = ArrayBuffer.fill(pteBytes)(0 : Byte)
    for ((b, idx) <- pte.ppa.toByteArray.reverse.zipWithIndex)
      bytes(idx) = b
    for ((b, idx) <- pte.tag.toByteArray.reverse.zipWithIndex)
      bytes(8 + idx) = b
    bytes(7) = (bytes(7) | ((pte.pageType & 0x3)         << 4)).toByte
    bytes(7) = (bytes(7) | (pte.allocated.compare(false) << 6)).toByte
    bytes(7) = (bytes(7) | (pte.used.compare(false)      << 7)).toByte

    bytes
  }

  implicit def LookupRequestSimAssign(a : AddressLookupRequestSim, req : AddressLookupRequest) = {
    req.seqId #= a.seqId
    req.tag #= a.tag
    req.reqType #= a.reqType
  }
  implicit def LookupRequestSimToBytes(req : AddressLookupRequestSim) : Seq[Byte] = {
    val pteBytes = 16
    val bytes = ArrayBuffer.fill(pteBytes)(0 : Byte)
    assign(req.tag, bytes, 0)
    assign(req.seqId, bytes, 6)
    bytes(5) = (bytes(5) | (req.reqType << 6)).toByte

    bytes
  }

  val legoMemHeaderCodec = (uint16 :: uint8 :: uint8 :: uint8 :: uint4 :: uint4 :: int16).as[LegoMemHeaderSim]
  val legoMemMsgCodec = (legoMemHeaderCodec :: bytes(56)).as[(LegoMemHeaderSim, ByteVector)]
  val legoMemAccessMsgCodec = (legoMemHeaderCodec :: int64 :: bytes(48)).as[(LegoMemHeaderSim, Long, ByteVector)]

  implicit def simStructToBigInt(s : MemSimStruct) : BigInt = {
    BigInt(s.asBytes.toArray)
  }

}

