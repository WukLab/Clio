package wuklab.sim


import java.math.BigInteger

import wuklab._
import spinal.core.sim._
import spinal.lib._
import spinal.core._

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
                            ){}

object SimConversions {
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

  implicit def simStructToBigInt(s : MemSimStruct) : BigInt = {
    BigInt(s.asBytes.toArray)
  }

}

