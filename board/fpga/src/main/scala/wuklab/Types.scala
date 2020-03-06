package wuklab

import spinal.core._
import spinal.lib.bus.amba4.axi.Axi4Config
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
  val  hashtableBaseAddr : BigInt
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
//    assert(false, f"VA2PA ${pte.ppa.getWidth} $ppaWidth, $ppaOffsetWidth, $physicalAddrWidth")

    val mask = ~getMask(pte.pageType)
    val pa = (pte.ppa << ppaOffsetWidth) | (va.resize(physicalAddrWidth) & mask.resize(physicalAddrWidth))
    // tODO: check this
    pa.resize(physicalAddrWidth bits)
  }

  def genTag(pid : UInt, va : UInt) : Bits = {
    // TODO: change this
    (pid ## va).resize(tagWidth bits)
  }

  // bus Config
  def lookupAxi4Config = Axi4Config(
    addressWidth = hashtableAddrWidth,
    dataWidth = 512,
    useId = false,
    useRegion = false,
    useBurst = true,
    useLock = false,
    useQos = false,
    useLen = true,
    useResp = true,
    useProt = false,
    useCache = false,
    useStrb = false
  )
  def accessAxi4Config = Axi4Config (
    addressWidth = physicalAddrWidth,
    dataWidth = 512,
    useId = false,
    useRegion = false,
    useQos = false
  )

  def destWidth = 4
  def dmaLengthWidth = 16

  def dmaAxisConfig = AxiStreamConfig (512)
  def epAxisConfig = AxiStreamConfig (512, destWidth = destWidth)

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

trait Header[T <: Header[T]] {
  val packedWidth : Int
  def packedBytes : Int = packedWidth / 8
  def getSize : UInt
  def fromWiderBits(bits : Bits) : T
}

object LegoMemHeader {
  // Need a mapper function at bits
  val headerWidth = 64
  def apply(bits: Bits): LegoMemHeader = {
    assert(bits.getWidth >= headerWidth, "Try to get LegoMem from a small bits")
    val header = LegoMemHeader()
    header.assignFromBits(bits(bits.getWidth downBy headerWidth))
    header
  }

  def popContFromBits(bits : Bits) : (Bits, UInt) = {
    val next = cloneOf(bits)
    val header = apply(bits)
    val (nextHeader, dest) = header.stackPop
    next := bits
    next.allowOverride
    next(bits.getWidth - headerWidth, headerWidth bits) := nextHeader.asBits
    (next, dest)
  }
}

case class LegoMemHeader() extends Bundle with Header[LegoMemHeader] {
  val pid       = UInt(16 bits)
  val tag       = UInt(8 bits)
  val reqType   = UInt(8 bits)
  val cont      = UInt(8 bits)
  val reqStatus = UInt(4 bits)
  val seqId     = UInt(4 bits)
  val size      = UInt(16 bits)

  def stackPop: (LegoMemHeader, UInt) = {
    val next = cloneOf(this)
    val nextAddr = cont(7 downto 4)
    next := this
    next.tag.allowOverride
    next.tag := this.tag |<< 4
    (this, nextAddr)
  }
  def stackPush (nextAddr : UInt): Unit = {
    assert (nextAddr.getWidth == 4)

    cont(7 downto 4) := nextAddr
    tag := tag |>> 4
  }
  def header = this

  override val packedWidth = LegoMemHeader.headerWidth
  override def getSize = size
  override def fromWiderBits(bits: Bits) = LegoMemHeader.apply(bits)

  override def assignFromBits(bits: Bits) = {
    assert(bits.getWidth == packedWidth, "LegoMem Build: Width mismatch")

    pid       := bits(48, 16 bits).asUInt
    tag       := bits(40, 8 bits).asUInt
    reqType   := bits(32, 8 bits).asUInt
    cont      := bits(24, 8 bits).asUInt
    reqStatus := bits(20, 4 bits).asUInt
    seqId     := bits(16, 4 bits).asUInt
    size      := bits(0, 16 bits).asUInt
  }

  override def asBits = {
    val bits = Bits(packedWidth bits)
    bits(48, 16 bits) := pid      .asBits
    bits(40, 8 bits)  := tag      .asBits
    bits(32, 8 bits)  := reqType  .asBits
    bits(24, 8 bits)  := cont     .asBits
    bits(20, 4 bits)  := reqStatus.asBits
    bits(16, 4 bits)  := seqId    .asBits
    bits(0, 16 bits)  := size     .asBits
    bits
  }

}

case class LegoMemAccessHeader(virtAddrWidth : Int) extends Bundle with Header[LegoMemAccessHeader]{
  val header    = LegoMemHeader()
  val addr      = UInt(virtAddrWidth bits)

  override val packedWidth = 64 + header.packedWidth
  override def getSize = header.size
  override def fromWiderBits(bits: Bits) = {
    assert(bits.getWidth >= packedWidth)
    val next = cloneOf(this)
    next.header := LegoMemHeader.apply(bits)
    next.addr := bits(bits.getWidth - packedWidth, virtAddrWidth bits).asUInt
    next
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

}


// INTO and out of Sequencer
case class AddressLookupRequest(tagWidth : Int) extends Bundle {
  val seqId     = UInt(16 bits)
  val reqType   = UInt(2 bits)
  val tag       = UInt(tagWidth bits)

  // This methods are for test only
  override def asBits : Bits = {
    val bits = Bits(64 bits)
    bits(45 downto 0) := tag.asBits.resize(46 bits)
    bits(47 downto 46) := reqType.asBits
    bits(63 downto 48) := seqId.asBits
    bits
  }

  override def assignFromBits(bits: Bits): Unit = {
    assert(bits.getWidth == 64)
    tag     := bits(45 downto 0).asUInt.resize(tagWidth bits)
    reqType := bits(47 downto 46).asUInt
    seqId   := bits(63 downto 48).asUInt
  }
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

