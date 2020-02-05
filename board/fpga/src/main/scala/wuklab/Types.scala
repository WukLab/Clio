package wuklab

import spinal.core._
import wuklab.Utils._

case class LegoMemConfig (
                           numCacheCells : Int,
                           physicalAddrWidth : Int,
                           virtualAddrWidth : Int,
                           virtualTagWidth : Int,
                           tagWidth : Int,
                           ppaWidth : Int
                         ){
}

object MemoryRequestType {
  val write = 0x02
  val writeResp = 0x03
  val read = 0x04
  val readResp = 0x05
  val alloc = 0x06
  val allocResp = 0x07
  val free = 0x08
  val freeResp = 0x09
}

object MemoryRequestStatus {
  val okay = 0x00
  val errInvalid = 0x01
  val errPermission = 0x02
}

case class LegoMemHeader(addrWidth : Int) extends Bundle {
  val pid       = UInt(16 bits)
  val seqId     = UInt(16 bits)
  val reqType   = UInt(8 bits)
  val reqParam  = UInt(8 bits)
  val size      = UInt(16 bits)
  val addr      = UInt(addrWidth bits)
  // Network infomation
  val srcIp     = UInt(32 bits)
  val srcPort   = UInt(8 bits)
}

case class InternalMemoryRequest(addrWidth : Int) extends Bundle {
  val pid       = UInt(16 bits)
  val seqId     = UInt(16 bits)
  val reqType   = UInt(8 bits)
  val reqParam  = UInt(8 bits)
  val size      = UInt(16 bits)
  val addr      = UInt(addrWidth bits)

  def mask : UInt = {
    U"64'hfffff_ffff"
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

object PageTableEntry {
  def compact(pte : PageTableEntry, bits : Bits) : PageTableEntry = {
    val next = cloneOf(pte)
    next.fromCompactBits(bits)
    next
  }
}

// TODO: optional abstract
case class PageTableEntry(
                           usePpa : Boolean = true,
                           ppaWidth : Int = 0,
                           useTag : Boolean = true,
                           tagWidth : Int = 0,
                           usePteAddr : Boolean = false,
                           useHitAddr : Boolean = false
                         ) extends Bundle {

  assert (!useTag || (useTag && tagWidth != 0))
  assert (!usePpa || (usePpa && ppaWidth != 0))

  val seqId = UInt(16 bits)
  val ppa = if (usePpa) UInt(48 bits) else null
  val used = Bool
  val valid = Bool
  val pageType = UInt(2 bits)

  val tag = if (useTag) UInt(ppaWidth bits) else null

  val pteAddr = if (usePteAddr) UInt(16 bits) else null
  val hitAddr = if (useHitAddr) UInt(16 bits) else null

  // This function decides the physical layout of a PTE
  val packedWidth : Int = 128
  override def asBits : Bits = {
    assert(usePpa && useTag)

    val bits = Bits(packedWidth bits)
    bits(64 countBy tagWidth) := tag.asBits
    bits(0 countBy ppaWidth) := ppa.asBits
    bits(63) := used
    bits(62) := valid
    bits(61 downto 60) := pageType.asBits
    bits
  }

  def fromBits(bits : Bits) = {
    assert(bits.getWidth == packedWidth)

    tag := bits(64 countBy tagWidth).asUInt
    ppa := bits(0 countBy ppaWidth).asUInt
    used := bits(63)
    valid := bits(62)
    pageType := bits(61 downto 60).asUInt
    this
  }

  val compactWidth : Int = ppaWidth + 4
  def asCompactBits : Bits = {
    val bits = Bits(compactWidth bits)
    bits(0 countBy ppaWidth) := ppa.asBits
    bits(ppaWidth) := used
    bits(ppaWidth + 1) := valid
    bits(ppaWidth + 2 countBy 2) := pageType.asBits
    bits
  }
  def fromCompactBits(bits : Bits) = {
    assert(bits.getWidth == compactWidth)
    ppa := bits(0 countBy ppaWidth).asUInt
    used := bits(ppaWidth)
    valid := bits(ppaWidth + 1)
    pageType := bits(ppaWidth + 2 countBy 2).asUInt
    this
  }

  def entryAddr : UInt = pteAddr * 128

  def := (entry: PageTableEntry) = {
    // a complex assignment
    used := entry.used
    valid := entry.valid
    pageType := entry.pageType
    seqId := entry.seqId

    if (usePpa) ppa := (if (entry.usePpa) entry.ppa else U"0")
    if (useTag) tag := (if (entry.useTag) entry.tag else U"0")
    if (usePteAddr) pteAddr := (if (entry.usePteAddr) entry.pteAddr else U"0")
    // TODO: other fields
  }

}


// INTO and out of Sequencer
case class AddressLookupRequest(addrWidth : Int) extends Bundle {
  val pid       = UInt(16 bits)
  val seqId     = UInt(16 bits)
  val reqType   = UInt(2 bits)
  val va        = UInt(addrWidth bits)

  def tag : UInt = {
    va
  }
}

case class AddressLookupResult(addrWidth : Int) extends Bundle {
  val pid       = UInt(16 bits)
  val seqId     = UInt(16 bits)
  val pte       = PageTableEntry(ppaWidth = addrWidth, usePpa = true, useTag = false)
}

object DataMoverCmd {
  // create basic commands
  def apply(addr : UInt, btt : UInt, tag : UInt = 0, isIncr : Bool = True): DataMoverCmd = {
    val data = apply(addr.getWidth, useBasic = true)
    data.addr := addr
    data.btt := btt
    data.isIncr := isIncr
    data.tag := 0
    data
  }
}

case class DataMoverCmd(addrWidth : Int, useBasic : Boolean) extends Bundle {
  // verify the addr is byte aligned
  assert (addrWidth % 8 == 0)

  val tag = UInt(4 bits)
  val addr = UInt(addrWidth bits)
  val isIncr = Bool
  val btt = UInt(23 bits)

  val cache = if (!useBasic) UInt(4 bits) else null
  val user  = if (!useBasic) UInt(4 bits) else null
  val drr   = if (!useBasic) Bool else null
  val eof   = if (!useBasic) Bool else null
  val dsa   = if (!useBasic) UInt(6 bits) else null

  override def asBits : Bits = {
    val bits = if (useBasic) Bits(addrWidth + 40 bits) else Bits(addrWidth + 48 bits)
    if (!useBasic) {
      bits(addrWidth + 47 downto addrWidth + 44) := cache.asBits
      bits(addrWidth + 43 downto addrWidth + 40) := user.asBits
      bits(31) := drr
      bits(30) := eof
      bits(29 downto 24) := dsa.asBits
    } else {
      // these fields are all rsvd
      bits(31) := False
      bits(30) := False
      bits(29 downto 24) := 0
    }
    bits(addrWidth + 39 downto addrWidth + 36) := 0 // Reserved
    bits(addrWidth + 47 downto addrWidth + 44) := tag.asBits
    bits(addrWidth + 31 downto 32) := addr.asBits
    bits(23) := isIncr
    bits(22 downto 0) := btt.asBits
    bits
  }

  override def assignFromBits(bits: Bits): Unit = {
    if (useBasic)
      assert(Bits.getWidth == addrWidth + 40)
    else
      assert(Bits.getWidth == addrWidth + 48)

    if (!useBasic) {
      cache := bits(addrWidth + 47 downto addrWidth + 44).asUInt
      user := bits(addrWidth + 43 downto addrWidth + 40).asUInt
      drr := bits(31)
      eof := bits(30)
      dsa := bits(29 downto 24).asUInt
    }
    tag := bits(addrWidth + 47 downto addrWidth + 44).asUInt
    addr := bits(addrWidth + 31 downto 32).asUInt
    isIncr := bits(23)
    btt := bits(22 downto 0).asUInt
  }
}

