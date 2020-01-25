package wuklab

import spinal.core._

object MemoryRequestType {
  val write = 0x01
  val read = 0x02
  val alloc = 0x03
  val free = 0x04
}

// INTO and out of Sequencer
case class InternalMemoryRequest(addrWidth : Int) extends Bundle {
  val pid       = UInt(16 bits)
  val seqId     = UInt(16 bits)
  val reqType   = UInt(8 bits)
  val reqParam  = UInt(8 bits)
  val size      = UInt(16 bits)
  val addr      = UInt(addrWidth bits)

  def mask : UInt = {
    reqParam
  }
}

object ControlRequest {
  def apply(
             cid : UInt,
             param8 : UInt = 0,
             param32 : UInt = 0
           ): ControlRequest = {
    val req = apply()
    req.cid := cid
    req.param8 := param8
    req.param32 := param32
    req
  }
}

case class ControlRequest() extends Bundle {
  val cid = UInt(8 bits)
  val addr = UInt(8 bits)
  val cmd = UInt(4 bits)
  val beats = UInt(4 bits)
  val param8  = UInt(8 bits)
  val param32 = UInt(32 bits)
}

// From lookup result to Memory Access Unit
case class AccessCommand(addrWidth : Int) extends Bundle {
  val status = UInt(2 bits)
  val cmd = UInt(4 bits)
  val seqId = UInt(16 bits)
  val phyPage = UInt(addrWidth bit)
}

case class PTE(addrWidth : Int) extends Bundle {
  val used = Bool
  val valid = Bool
  val tag = UInt(48 bits)
  val pageType = UInt(2 bits)
  val ppa = UInt(48 bits)
}

case class PTELookupResult(usePteAddr : Boolean, useHitAddr : Boolean) extends Bundle {
  val pte = PTE(48)
  val isMatch = Bool
  val pteAddr = if (usePteAddr) UInt(16 bits)
  val hitAddr = if (useHitAddr) UInt(16 bits)
}

