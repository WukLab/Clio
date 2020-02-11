package wuklab

import spinal.core._
import wuklab.Utils._

trait CoreMemConfig {
  // Memory service config
  val  physicalAddrWidth : Int
  val  virtualAddrWidth : Int
  val  hashtableAddrWidth : Int
  val  tagWidth : Int
  val  ppaWidth : Int
  val  pageSizes : Seq[Int]
  // Cache config
  val  numCacheCells : Int
  val  numPageFaultCacheCells : Int
  // Hash Table Config
  // | ------------ Physical Address Width -----  |
  // | ------ | -----  Hash Table Address Width - |
  // | ------ | Pte Addr Width | Pte offset Width |
  val  hashtableBaseAddr : Int
  val  pteAddrWidth : Int    // The minimal bits of PTE addr

  val  ptePerLine : Int
  val  ptePerBucket : Int

  // Help Methods
  // Page Table Related functions
  def pageFaultCellWidth = log2Up(numPageFaultCacheCells)
  def numPageSizes = pageSizes.size

  def linePerBucket = ptePerBucket / ptePerLine
  def getPteBusAddr(addr : UInt): UInt = {
    assert(addr.getWidth == pteAddrWidth, "Wrong Pte Addr Width")
    U(hashtableBaseAddr, hashtableAddrWidth bits) + (addr << bucketAddrOffsetWidth)
  }

  def pageOffsets = pageSizes.map(log2Up(_))
  def pageMasks : Seq[UInt] = pageOffsets.map(i => {
    val mask = UInt(tagWidth bits)
    mask(tagWidth-1 downto i).setAll()
    mask(i-1 downto 0).clearAll()
    mask
  })
  def getMask(pageType : UInt) : UInt = {
    assert(pageMasks.size == pageSizes.size)
    val masks = pageMasks.zipWithIndex.map(_.swap)
    pageType.muxList(U(0, tagWidth bits), masks)
  }
  // Core component
//  assert(isPow2(numCacheCells), "Number of memory cells should be power of 2")
  def cacheCellWidth = log2Up(numCacheCells)

  // Types
  def lookupReqType = AddressLookupRequest(tagWidth)

  def pteWithAddrType = cloneOf(PageTableEntry(ppaWidth = ppaWidth, tagWidth = tagWidth, pteAddrWidth = pteAddrWidth))
  def pteResType = cloneOf(PageTableEntry(ppaWidth = ppaWidth, useTag = false))
  def pteWithPpaType = cloneOf(PageTableEntry(ppaWidth = ppaWidth, tagWidth = tagWidth))

  def pteFullWidth = pteWithAddrType.packedWidth
  def pteFullBytes = pteFullWidth / 8
  def pteCompactWidth = pteWithAddrType.compactWidth

  def pteAddrOffsetWidth = log2Up(pteFullBytes)
  def bucketAddrOffsetWidth = log2Up(pteFullBytes * ptePerBucket)

  def ppaOffsetWidth = physicalAddrWidth - ppaWidth

  // Memory Access
  def va2pa(pte : PageTableEntry, va : UInt): UInt = {
    assert(va.getWidth == virtualAddrWidth)
    assert(pte.usePpa)

    val mask = ~getMask(pte.pageType)
    (pte.ppa << ppaOffsetWidth) | (va.resize(physicalAddrWidth) & mask.resize(physicalAddrWidth))
  }
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

case class LegoMemHeader(virtAddrWidth : Int) extends Bundle {
  val pid       = UInt(16 bits)
  val seqId     = UInt(16 bits)
  val reqType   = UInt(8 bits)
  val reqParam  = UInt(8 bits)
  val size      = UInt(16 bits)
  val addr      = UInt(virtAddrWidth bits)
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
             cmd : UInt = 0,
             beats : UInt = 1,
             param8 : UInt = 0,
             param32 : UInt = 0,
             addr : UInt = 0
           ): ControlRequest = {
    val req = apply()
    req.addr := addr
    req.cid := cid
    req.cmd := cmd
    req.beats := beats
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

  override def asBits : Bits = {
    val bits = Bits(64 bits)
    bits(31 downto 0) := param32.asBits
    bits(39 downto 32) := param8.asBits
    bits(43 downto 40) := beats.asBits
    bits(47 downto 44) := cmd.asBits
    bits(55 downto 48) := addr.asBits
    bits(63 downto 56) := cid.asBits
    bits
  }

  override def assignFromBits(bits: Bits): Unit = {
    assert(bits.getWidth == 64)
    param32 := bits(31 downto 0).asUInt
    param8  := bits(39 downto 32).asUInt
    beats   := bits(43 downto 40).asUInt
    cmd     := bits(47 downto 44).asUInt
    addr    := bits(55 downto 48).asUInt
    cid     := bits(63 downto 56).asUInt
  }
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
    next.seqId := pte.seqId
    if (next.useTag) next.tag := pte.tag
    if (next.usePteAddr) next.pteAddr := pte.pteAddr
    next
  }
  def full(pte : PageTableEntry, bits : Bits) : PageTableEntry = {
    val next = cloneOf(pte)
    next := pte
    next.fromFullBits(bits)
    next
  }
}

// TODO: optional abstract
case class PageTableEntry(
                           usePpa : Boolean = true,
                           ppaWidth : Int = 0,
                           useTag : Boolean = true,
                           tagWidth : Int = 0,
                           pteAddrWidth : Int = 0
                         ) extends Bundle {

  assert (!useTag || (useTag && tagWidth != 0))
  assert (!usePpa || (usePpa && ppaWidth != 0))

  def usePteAddr = pteAddrWidth > 0

  val seqId = UInt(16 bits)
  val used = Bool
  val allocated = Bool
  val pageType = UInt(2 bits)

  val ppa = if (usePpa) UInt(ppaWidth bits) else null
  val tag = if (useTag) UInt(tagWidth bits) else null
  val pteAddr = if (usePteAddr) UInt(pteAddrWidth bits) else null

  // This function decides the physical layout of a PTE
  val packedWidth : Int = 128
  def asFullBits : Bits = {
    assert(usePpa && useTag)

    val bits = Bits(packedWidth bits)
    bits := 0
    bits(64 countBy tagWidth) := tag.asBits
    bits(0 countBy ppaWidth) := ppa.asBits
    bits(63) := used
    bits(62) := allocated
    bits(61 downto 60) := pageType.asBits
    bits
  }

  def fromFullBits(bits : Bits) = {
    assert(bits.getWidth == packedWidth)

    tag := bits(64 countBy tagWidth).asUInt
    ppa := bits(0 countBy ppaWidth).asUInt
    used := bits(63)
    allocated := bits(62)
    pageType := bits(61 downto 60).asUInt
    this
  }

  val compactWidth : Int = ppaWidth + 4
  def asCompactBits : Bits = {
    val bits = Bits(compactWidth bits)
    bits(0 countBy ppaWidth) := ppa.asBits
    bits(ppaWidth) := used
    bits(ppaWidth + 1) := allocated
    bits(ppaWidth + 2 countBy 2) := pageType.asBits
    bits
  }
  def fromCompactBits(bits : Bits) = {
    assert(bits.getWidth == compactWidth)
    ppa := bits(0 countBy ppaWidth).asUInt
    used := bits(ppaWidth)
    allocated := bits(ppaWidth + 1)
    pageType := bits(ppaWidth + 2 countBy 2).asUInt
    this
  }

  def entryAddr : UInt = pteAddr * 128

//  def := (entry: PageTableEntry) = {
//    // a complex assignment
//    used := entry.used
//    allocated := entry.allocated
//    pageType := entry.pageType
//    seqId := entry.seqId
//
//    if (usePpa) ppa := (if (entry.usePpa) entry.ppa else U"0")
//    if (useTag) tag := (if (entry.useTag) entry.tag else U"0")
//    if (usePteAddr) pteAddr := (if (entry.usePteAddr) entry.pteAddr else U"0")
//  }

}


// INTO and out of Sequencer
case class AddressLookupRequest(tagWidth : Int) extends Bundle {
  val seqId     = UInt(16 bits)
  val reqType   = UInt(2 bits)
  val tag       = UInt(tagWidth bits)
}

case class AddressLookupResult(addrWidth : Int) extends Bundle {
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

