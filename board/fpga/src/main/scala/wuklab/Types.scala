package wuklab

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4Config
import wuklab.Utils._

// TODO: extra shootdown bits
object LegoMem {
  object RequestType {
    def apply()    = UInt(8 bits)
    // 00 -> 3F : Basic MGNT
    def INVALID    = U"8'h00"
    def LOOPBACK   = U"8'h01"
    def PINGPONG   = U"8'h02"
    def BARRIER    = U"8'h10"

    // 40 -> 7F : APP
    def READ       = U"8'h40"
    def READ_RESP  = U"8'h41"
    def READ_TAS   = U"8'h42"
    def READ_DEREF_WRITE = U"8'h43"
    def READ_DEREF_READ  = U"8'h44"
    def READ_MIG   = U"8'h45"

    def WRITE      = U"8'h50"
    def WRITE_RESP = U"8'h51"

    def CACHE_SHOOTDOWN = U"8'h70"

    // 80 -> FF :
    def ALLOC      = U"8'h85"
    def ALLOC_RESP = U"8'h86"
    def FREE       = U"8'h87"
    def FREE_RESP  = U"8'h88"


    def resp(u: UInt) : UInt = {
      require(u.getWidth == 8, "Not valid request")
      u.mux(
        READ    -> READ_RESP,
        WRITE   -> WRITE_RESP,
        READ_MIG -> WRITE,
        default -> INVALID
      )
    }

    def isControlRequest(u : UInt) : Bool = {
      require(u.getWidth == 8, "Not valid request")
      u(7)
    }
  }

  object RequestStatus {
    def apply()         = UInt(4 bits)
    def OKAY            = U"4'h00"
    def ERR_INVALID     = U"4'h01"
    def ERR_WRITE_PERM  = U"4'h02"
    def ERR_READ_PERM   = U"4'h03"
  }

  object Continuation {
    def apply()         = UInt(16 bits)
    def apply(c1: UInt, c2: UInt = EP_DROP, c3: UInt = EP_DROP, c4: UInt = EP_DROP) = (c4 ## c3 ## c2 ## c1).asUInt
    def EP_DROP         = U"4'h0"
    def EP_NETWORK      = U"4'h0"
    def EP_COREMEM      = U"4'h1"
    def EP_SOC          = U"4'h2"
    def EP_KEYVALUE     = U"4'h5"
  }
}

trait LegoMemConfig {
  val epDataWidth = 512
  val epDataBytes = epDataWidth / 8

  val epCtrlWidth = 64
  val epCtrlBytes = epCtrlWidth / 8

  def destWidth = 4
  def dmaLengthWidth = 16

  def dmaAxisConfig = AxiStreamConfig (512)
  def epDataAxisConfig = AxiStreamConfig (512, destWidth = destWidth)
  def epCtrlAxisConfig = AxiStreamConfig (64, destWidth = destWidth)
}

trait CoreMemConfig extends LegoMemConfig {

  // === Memory service config
  val physicalAddrWidth : Int
  val virtualAddrWidth : Int

  // === Virtual Memory Page Config
  // --- Page Sizes
  val pageSizes : Seq[Int]

  def numPageSizes = pageSizes.size
  def pageOffsets = pageSizes.map(log2Up(_))

  // --- Tag is the size of the input lookup
  val fullTagWidth : Int
  val tagOffset : Int

  def tagWidth : Int = fullTagWidth - tagOffset
  def genTag(pid : UInt, va : UInt) : Bits = {
    ((pid ## va) >> tagOffset).resize(tagWidth bits)
  }
  def tag2fullTag(tag : UInt) : UInt = {
    (tag ## U(0, tagOffset bits)).asUInt
  }
  def fullTagMasks : Seq[UInt] = pageOffsets.map(i => {
    val mask = UInt(fullTagWidth bits)
    mask(fullTagWidth-1 downto i).setAll()
    mask(i-1 downto 0).clearAll()
    mask
  })
  def getFullTagMask(pageType : UInt) : UInt = {
    assert(fullTagMasks.size == pageSizes.size)
    val masks = fullTagMasks.zipWithIndex.map(_.swap)
    pageType.muxList(U(0, fullTagWidth bits), masks)
  }
  def getTagMask(pageType: UInt) = getFullTagMask(pageType) >> tagOffset
  def getPAMask(pageType: UInt) = getFullTagMask(pageType).resize(physicalAddrWidth)

  // --- Cache config
  val numCacheCells : Int
  val numPageFaultCacheCells : Int

  def cacheCellWidth = log2Up(numCacheCells)
  def pageFaultCacheCellWidth = log2Up(numPageFaultCacheCells)

  // === Hash Table Config
  // Hash Table should be
  // | ------------ Physical Address Width -----  |
  // | ------ | -----  Hash Table Address Width - |
  // | ------ | Pte Addr Width | Pte offset Width |
  val hashtableBaseAddr : BigInt
  // numBuckets * ptePerBuckets = numPte
  val numBuckets : Int
  val ptePerBucket : Int

  def bucketNumberWidth : Int = log2Up(numBuckets)
  def ptePerLine : Int = 4
  def linePerBucket = ptePerBucket / ptePerLine

  def inBucketAddrWidth = log2Up(pteFullBytes * ptePerBucket)
  def getBucketAddr(bucketNum : UInt): UInt = {
    assert(bucketNum.getWidth == bucketNumberWidth, "Wrong Pte Addr Width")
    U(hashtableBaseAddr, physicalAddrWidth bits) + (bucketNum << inBucketAddrWidth).resize(physicalAddrWidth)
  }

  // === CoreMem Component
  // --- Types
  def lookupReqType = AddressLookupRequest(tagWidth)

  def pteWithAddrType = cloneOf(PageTableEntry(ppaWidth = physicalAddrWidth, tagWidth = tagWidth, pteAddrWidth = bucketNumberWidth))
  def pteResType = cloneOf(PageTableEntry(ppaWidth = physicalAddrWidth, useTag = false))
  def pteWithPpaType = cloneOf(PageTableEntry(ppaWidth = physicalAddrWidth, tagWidth = tagWidth))

  // -- Data
  def pteFullWidth = pteWithAddrType.packedWidth
  def pteCompactWidth = pteWithAddrType.compactWidth
  def pteFullBytes = pteFullWidth / 8

  // Memory Access
  def va2pa(pte : PageTableEntry, va : UInt): UInt = {
    assert(va.getWidth == virtualAddrWidth)
    assert(pte.usePpa)

    val hiMask = getPAMask(pte.pageType)
    val loMask = ~hiMask

    (pte.ppa & hiMask) | (va.resize(physicalAddrWidth) & loMask)
  }

  // == Interface configuration
  def pageTableWriterAxi4Config = Axi4Config(
    addressWidth = physicalAddrWidth,
    dataWidth = pteWithAddrType.packedWidth,
    useId = false,
    useRegion = false,
    useBurst = false,
    useLock = false,
    useQos = false,
    useLen = false,
    useResp = true,
    useProt = false,
    useCache = false,
    useLast = false,
    useStrb = false
  )
  def lookupAxi4Config = Axi4Config(
    addressWidth = physicalAddrWidth,
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

  def printConfig: Unit = {
    println("Current CoreMemory Config:")
    println(s"- Bucket Number Width: $bucketNumberWidth")
    println(s"- Bucket Offset: $inBucketAddrWidth")
    println(s"- Total Page sizes: $numPageSizes")
    (pageSizes, pageOffsets).zipped foreach { (size, offset) =>
      println(s"  - Page size $size offset bits $offset")
    }
  }
}

trait Header[T <: Header[T]] {
  val packedWidth : Int
  def packedBytes : Int = packedWidth / 8
  def getSize : UInt
  def fromWiderBits(bits : Bits) : T
}

object LegoMemHeader {
  // Need a mapper function at bits
  val headerWidth = 128
  val headerBytes = headerWidth / 8
  def apply(bits: Bits): LegoMemHeader = {
    assert(bits.getWidth >= headerWidth, "Try to get LegoMem from a small bits")
    val header = LegoMemHeader()
    header.assignFromBits(bits(0, headerWidth bits))
    header
  }

  def popContFromBits(bits : Bits) : (Bits, UInt) = {
    val next = cloneOf(bits)
    val header = apply(bits)
    val (nextHeader, dest) = header.stackPop
    next := bits
    next.allowOverride
    next(0, headerWidth bits) := nextHeader.asBits
    (next, dest)
  }

  def toBitsOperation(f : LegoMemHeader => LegoMemHeader) : Bits => Bits = {
    bits : Bits => {
      val next = cloneOf(bits)
      next := bits
      next.allowOverride
      next(0, headerWidth bits) := f(apply(bits)).asBits
      next
    }
  }

  def assignToBitsOperation(f : LegoMemHeader => Unit) : Bits => Bits = {
    toBitsOperation(header => {
      val next = cloneOf(header)
      next := header
      next.allowOverride
      f(next)
      next
    })
  }

}

// TODO: add space for redirection, should expend the parameters
case class LegoMemHeader() extends Bundle with Header[LegoMemHeader] {
  val pid       = UInt(16 bits)
  val tag       = UInt(8 bits)
  val reqType   = UInt(8 bits)
  val reqStatus = UInt(4 bits)
  val flagRoute = Bool
  val flagRepl  = Bool
  val rsvd      = UInt(2 bits)
  val seqId     = UInt(8 bits)
  val size      = UInt(16 bits)

  val cont      = UInt(16 bits)
  val srcPort   = UInt(8 bits)
  val destPort  = UInt(8 bits)
  val destIp    = UInt(32 bits)

  def stackPop: (LegoMemHeader, UInt) = {
    val next = cloneOf(this)
    val nextAddr = cont(3 downto 0)
    next := this
    next.cont.allowOverride
    next.cont := cont |>> 4
    (next, nextAddr)
  }

  def stackPush (nextAddr : UInt): Unit = {
    assert (nextAddr.getWidth == 4)

    cont.allowOverride
    cont := cont |<< 4
    cont(4 downto 0) := nextAddr
  }

  def header = this

  override val packedWidth = LegoMemHeader.headerWidth
  override def getSize = size
  override def fromWiderBits(bits: Bits) = LegoMemHeader.apply(bits)

  override def assignFromBits(bits: Bits) = {
    assert(bits.getWidth == packedWidth, "LegoMem Build: Width mismatch")

    pid       := bits(0,  16 bits).asUInt
    tag       := bits(16, 8  bits).asUInt
    reqType   := bits(24, 8  bits).asUInt
    reqStatus := bits(32, 4  bits).asUInt
    flagRoute := bits(36)
    flagRepl  := bits(37)
    rsvd      := bits(38, 2  bits).asUInt
    seqId     := bits(40, 8  bits).asUInt
    size      := bits(48, 16 bits).asUInt

    cont      := bits(64, 16 bits).asUInt
    srcPort   := bits(80, 8  bits).asUInt
    destPort  := bits(88, 8  bits).asUInt
    destIp    := bits(96, 32 bits).asUInt
  }

  override def asBits = {
    val bits = Bits(packedWidth bits)
    bits(0,  16 bits) := pid      .asBits
    bits(16, 8  bits) := tag      .asBits
    bits(24, 8  bits) := reqType  .asBits
    bits(32, 4  bits) := reqStatus.asBits
    bits(36)          := flagRoute
    bits(37)          := flagRepl
    bits(38, 2  bits) := rsvd     .asBits
    bits(40, 8  bits) := seqId    .asBits
    bits(48, 16 bits) := size     .asBits

    bits(64, 16 bits) := cont     .asBits
    bits(80, 8 bits)  := srcPort  .asBits
    bits(88, 8 bits)  := destPort .asBits
    bits(96, 32 bits) := destIp   .asBits
    bits
  }

}

case class LegoMemAccessHeader(virtAddrWidth : Int) extends Bundle with Header[LegoMemAccessHeader]{
  val header    = LegoMemHeader()
  val addr      = UInt(virtAddrWidth bits)
  val length    = UInt(32 bits)

  override val packedWidth = 96 + header.packedWidth
  override def getSize = header.size
  override def fromWiderBits(bits: Bits) = {
    assert(bits.getWidth >= packedWidth)
    val next = cloneOf(this)
    next.header := LegoMemHeader.apply(bits)
    next.addr   := bits(header.packedWidth, virtAddrWidth bits).asUInt
    next.length := bits(header.packedWidth + 64, 32 bits).asUInt
    next
  }
}

object ControlRequest {
  def apply(
             epid : UInt,
             cmd : UInt = 0,
             beats : UInt = 1,
             param8 : UInt = 0,
             param32 : UInt = 0,
             addr : UInt = 0
           ): ControlRequest = {
    val req = apply()
    req.addr := addr
    req.epid := epid.resize(8)
    req.cmd := cmd
    req.beats := beats
    req.param8 := param8
    req.param32 := param32
    req
  }

  def apply(bits : Bits): ControlRequest = {
    val req = ControlRequest()
    req.assignFromBits(bits)
    req
  }

}

case class ControlRequest() extends Bundle {
  val epid = UInt(8 bits)
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
    bits(63 downto 56) := epid.asBits
    bits
  }

  override def assignFromBits(bits: Bits): Unit = {
    assert(bits.getWidth == 64)
    param32 := bits(31 downto 0).asUInt
    param8  := bits(39 downto 32).asUInt
    beats   := bits(43 downto 40).asUInt
    cmd     := bits(47 downto 44).asUInt
    addr    := bits(55 downto 48).asUInt
    epid    := bits(63 downto 56).asUInt
  }

  def resizeParam(size : Int): UInt = {
    assert(size <= 40, "over sized prameter")
    (param8 ## param32).resize(size).asUInt
  }

  def assignParam(param : BitVector) : Unit = {
    assert(param.getWidth <= 40, "over sized prameter")
    param32 := param.asBits.asUInt.resize(32)
    if (param.getWidth > 32)
      param8 := param.asBits.asUInt.resize(40)(39 downto 32)
  }
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

object AddressLookupRequest {
  object RequestType {
    def apply     = UInt(2 bits)
    def LOOKUP    = U"00"
    def SHOOTDOWN = U"01"
  }
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

// Interface types
// slave: normal device
case class LegoMemControlEndpoint() extends Bundle with IMasterSlave {
  val in  = Stream (ControlRequest())
  val out = Stream (ControlRequest())

  override def asMaster(): Unit = {
    master (in)
    slave (out)
  }
}

