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

trait BitSerializable[T <: BitSerializable[T]] {
  self : T =>
  val codec : Codec[T]
  def serialize : BitVector = codec.encode(self).require
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
                              reqStatus : Int = 0,
                              flagRoute : Boolean = false,
                              flagRepl : Boolean = false,
                              rsvd : Int = 0,
                              seqId : Int = 0,
                              size : Int = 0,

                              cont : Int = 0,
                              destPort : Int = 0,
                              destIp : Long = 0
                            ) extends BitSerializable[LegoMemHeaderSim] {
  override val codec = SimConversions.legoMemHeaderCodec
}

// val length        = UInt(12 bits)
// val dest_port     = UInt(16 bits)
// val source_port   = UInt(16 bits)
// val ip_dest_ip    = UInt(32 bits)
// val ip_source_ip  = UInt(32 bits)
case class UDPHeaderSim(
                           sourceIp : Long,
                           destIP : Long,
                           sourcePort : Int,
                           destPort : Int,
                           length : Int
) extends BitSerializable[UDPHeaderSim] {
  override val codec = SimConversions.udpHeaderCodec
}

//bits(31 downto 0) := param32.asBits
//bits(39 downto 32) := param8.asBits
//bits(43 downto 40) := beats.asBits
//bits(47 downto 44) := cmd.asBits
//bits(55 downto 48) := addr.asBits
//bits(63 downto 56) := epid.asBits
case class ControlRequestSim (
                               param32 : Long = 0L,
                               param8 : Int = 0,
                               beats : Int = 1,
                               cmd : Int = 0,
                               addr : Int = 0,
                               epid : Int = 0
                             ) extends BitSerializable[ControlRequestSim] {
  override val codec = SimConversions.controlRequestCodec
}

case class KeyValueHeaderSim (
                              header : LegoMemHeaderSim,
                              keySize : Int = 0,
                              user : Int = 0,
                              valueSize : Int = 0
                             ) extends BitSerializable[KeyValueHeaderSim] {
  override val codec = SimConversions.keyValueHeaderCodec
}

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

  // We use a little eden in Host side, and get a big in arch
  // Shapeless codecs
  val legoMemHeaderCodec = (
         uint16L
      :: uint8L
      :: uint8L
      :: uint4L
      :: bool
      :: bool
      :: uint2
      :: uint8L
      :: uint16L
      // Routing infomation
      :: uint16L
      :: uint16L
      :: uint32L
    ).as[LegoMemHeaderSim]
  val legoMemAccessMsgCodec = (legoMemHeaderCodec :: int64L :: int32L :: bytes).as[(LegoMemHeaderSim, Long, Int, ByteVector)]
  val udpHeaderCodec = (uint32L :: uint32L :: uint16L :: uint16L :: uint16L).as[UDPHeaderSim]
  val controlRequestCodec = (uint32L :: uint8L :: uint4L :: uint4L :: uint8L :: uint8L).as[ControlRequestSim]
  val keyValueHeaderCodec = (legoMemHeaderCodec :: uint8L :: uint8L :: uint16L).as[KeyValueHeaderSim]
  val keyValueMessageCodes = (keyValueHeaderCodec :: bytes).as[(KeyValueHeaderSim, ByteVector)]

  implicit def simStructToBigInt(s : MemSimStruct) : BigInt = {
    BigInt(s.asBytes.toArray)
  }

  implicit def bitSerializableAssign[T <: BitSerializable[T]](b : BitSerializable[T], bits : spinal.core.Bits): Unit = {
    bits #= BigInt(b.serialize.toByteArray.reverse)
  }
  // Simple one!
  implicit def bitSerializableAxisAssign[T <: BitSerializable[T]]
  (b : BitSerializable[T], axiStreamPayload: AxiStreamPayload): Unit = {
    axiStreamPayload.tdata #= BigInt(b.serialize.toByteArray.reverse)
  }

  implicit def encodedToBigInt(a : Attempt[BitVector]) : BigInt = {
    BigInt(a.require.toByteArray)
  }

  implicit def encodedAssign(a : Attempt[BitVector], bits : spinal.core.Bits) : Unit = {
    bits #= encodedToBigInt(a)
  }

}

