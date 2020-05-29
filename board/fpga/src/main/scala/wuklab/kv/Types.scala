package wuklab.kv

import wuklab._
import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4Config

trait Compact[T1, T2] {
  def compactWidth : Int
  def compactType : Bits = Bits(compactWidth bits)
  def compress(t : T1) : Bits
  def decompress(bits: Bits) : T2
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

  // hash table config

  // dma config
  def dmaControllerReadConfig = ???
  def cmdControllerWriteConfig = ???

  def addrWidth = 32
  def sizeWidth = 16
  def keyWidth = 64

  // Hashtable configutations
  val hteEntryWidth = 8
  val hteNumEntries = 1 << hteEntryWidth
  val hteBucketOffset = 6
  def hashToAddress(hash : UInt) : UInt = (hash << hteBucketOffset).resize(addrWidth)

  // DMA latency (depends on the size)
  val dmaLatency = 64
  val dmaAddrWidth = 40
  val dmaConfig = AxiStreamDMAConfig(AxiStreamConfig(512, keepWidth = 64), addrWidth = addrWidth, lenWidth = sizeWidth)

  // Allocation
  def allocInterfaceType = AllocationInterface(sizeWidth, addrWidth, 2)
  val allocLatency = 64
  val allocEp : Int
  val allocAddr : Int

  // Physical Interface
  def accessAxi4Config = Axi4Config (
    addressWidth = dmaAddrWidth,
    dataWidth = 512,
    useId = false,
    useRegion = false,
    useQos = false
  )
}

// The Keyvalue LegoMem header
object KeyValueHeaderStorage extends Compact[KeyValueHeader, KeyValueReplyHeader] {
  override def compactWidth: Int = 160
  override def compress(t: KeyValueHeader): Bits = {
    val bits = Bits(compactWidth bits)
    bits := t.asBits
    bits
  }

  // TODO: add more fields
  override def decompress(bits: Bits): KeyValueReplyHeader = {
    assert(widthOf(bits) == compactWidth, "Width mismatch of the compacted result")
    val header = KeyValueReplyHeader()
    header.fromWiderBits(bits)
    header
  }
}
case class KeyValueHeader() extends Bundle with Header[KeyValueHeader] {
  val header = LegoMemHeader()
  val user = UInt(8 bits)
  val keySize = UInt(8 bits)
  val valueSize = UInt(16 bits)
//  val key = UInt(64 bits)

  override val packedWidth = header.packedWidth + 32
  override def getSize = header.getSize
  override def fromWiderBits(bits: Bits) = {
    assert(bits.getWidth >= packedWidth)
    val next = cloneOf(this)
    next.header := LegoMemHeader.apply(bits)
    next.user      := bits(header.packedWidth, 8 bits ).asUInt
    next.keySize   := bits(header.packedWidth + 8, 8 bits).asUInt
    next.valueSize := bits(header.packedWidth + 16, 16 bits).asUInt
    next
  }

//  def getHashTag = key
}

case class KeyValueReplyHeader() extends Bundle with Header[KeyValueReplyHeader] {
  val header = LegoMemHeader()
  val user = UInt(8 bits)
  val keySize = UInt(8 bits)
  val valueSize = UInt(16 bits)

  override val packedWidth = header.packedWidth + 32
  override def getSize = header.getSize
  override def fromWiderBits(bits: Bits) = {
    assignFromBits(bits)
    this
  }

}

// Internal Data structures
// 64 bit size
// 32 addr + flags, 16 version + 4 bit 2nd hash (last 4 bit)

// match function: given a KV Command & a PTE, print out a match

// Page Table Structure: |ptr|bitmap + version map + size + version|secondary hash|linked|next

// Get Hash    -> FireDMA Request -> Process Engine -> Insert (fetch PTE, KV, ll) (WB Cache?) -> PTE Commit
//             -> wait  kv FIFO   /                                                           -> Data Commit
//             -> wait cmd FIFO  /

// DMA Engine
// Virtual channel
case class KeyValueHashEntry() extends Bundle {
  val ptr     = UInt(32 bits)
  val flags = new Bundle {
    val valid   = Bool
  }
  val valueSize = UInt(16 bits)
  val keySize   = UInt(4 bits)
  val check     = Bits(8 bits)

  def physicalPtr(base : UInt) : UInt = base + (ptr << 4).resize(widthOf(base))
  def fullBytes = 8

  def digest(key : Bits) = check := key(7 downto 0)

  def asFull : Bits = {
    val bits = Bits(64 bits)
    bits := 0
    bits.allowOverride
    bits(31 downto 0)   := ptr      .asBits
    bits(47 downto 32)  := valueSize.asBits
    bits(51 downto 48)  := keySize  .asBits
    bits(52)            := flags.valid
    bits(63 downto 56)  := check
    bits
  }

  def fromFull(bits: Bits): KeyValueHashEntry = {
    assert(widthOf(bits) == 64, "Width mismatch of the valid")
    ptr       := bits(31 downto 0) .asUInt
    valueSize := bits(47 downto 32).asUInt
    keySize   := bits(51 downto 48).asUInt
    flags.valid := bits(52)
    check     := bits(63 downto 56)
    this
  }


}

object LegoMemKV {
  object bucketCommand {
    def apply() = UInt(8 bits)
    def READ   = U"8'h0"
    def WRITE  = U"8'h1"
    def UPDATE = U"8'h2"
    def DELETE = U"8'h3"

    def fromRequestCode(op : UInt) : UInt = (op & 0xF) |>> 1
    def toReplyCode(op : UInt) : UInt = (op |<< 1) + 0x61
  }

  object entryCommand {
    def apply() = UInt(8 bits)
    def READ   = U"8'h0"
    def WRITE  = U"8'h1"
    def UPDATE = U"8'h2"
    def DELETE = U"8'h3"
  }

  object ctrlCommand {
    def ALLOC_KV_ONLY = U"4'h8"
    def ALLOC_KV_AND_HTE = U"4'h9"
  }

  object Size {
    def apply() = UInt(8 bits)
    def SIZE16  = U"4'h0"
    def SIZE32  = U"4'h1"
    def SIZE64  = U"4'h2"
    def SIZE128 = U"4'h3"
    def SIZE256 = U"4'h4"
    def SIZE512 = U"4'h5"
  }
  object Id {
    def apply() = UInt(16 bits)
  }
}

// Lock and unlock

// Commands:
//  PROCESS
//    READ
//    WRITE
//  CHECK AND INSTALL
//    CHECK AND READ
//    CHECK AND UPDATE
//    CHECK AND RETURN
//  CHECK AND SEND?

// versioned WRITE -> WIRTE, CHECK AND UPDATE
// versioned READ -> READ, SELECT TOP VERSION, AND UPDATE

// PUT? match -> insert

// We must lock on PTE
// We give all PTE Writes
case class KeyValueBucketCommand() extends Bundle {
  // keysize < 512

  val cmd       = LegoMemKV.bucketCommand()
  val keySize   = UInt(4 bits)
  val valueSize = UInt(16 bits)
  // This is for PTE Addr. The last bit is for valid
  val id        = LegoMemKV.Id()
  // This is for write back
  val entryAddr = UInt(32 bits)
}

case class KeyValueBucketCommandInterface(useBucket : Boolean = false) extends Bundle with IMasterSlave {
  val header = Stream (KeyValueBucketCommand())
  val data   = Stream Fragment Bits(512 bits)
  val bucket = if (useBucket) Stream Fragment(Bits(512 bits)) else null

  override def asMaster(): Unit = {
    master (header)
    master (data)
    if (useBucket) master(bucket)
  }
}

case class KeyValueEntryCommand() extends Bundle {
  import LegoMemKV.entryCommand._
  val cmd       = LegoMemKV.entryCommand()
  val last      = Bool // If is last of line
  val hasNext   = Bool // If has chained
  val matched   = Bool // If is matched
  val entryValid = Bool // if the current entry addr is valie
  def loopback  = Mux(is(WRITE), !entryValid && hasNext, last && hasNext)

  val id        = UInt(16 bits) // for lock
  val entrySlot = UInt(4 bits)
  val entryAddr = UInt(32 bits)
  val linkAddr  = UInt(32 bits)
  val entry     = KeyValueHashEntry()

  def needMatch = is(READ, UPDATE, DELETE) && entry.flags.valid // if need match
  def needFetchedData = is(READ) && entry.flags.valid // if need match
  def needInputData  = is(WRITE, UPDATE)

  def is(commands : UInt *) = commands.map(_ === cmd).orR
  def kvSize : UInt = entry.valueSize + 8
  def fetchSize : UInt = Mux(cmd === READ, kvSize, 8)

  def entrySlotAddr = entryAddr + (entrySlot << 3)
}

case class KeyValueEntryCommandInterface() extends Bundle with IMasterSlave {
  val header = slave Stream KeyValueEntryCommand()
  val data   = slave Stream Fragment(Bits(512 bits))

  override def asMaster(): Unit = {
    master (header)
    master (data)
  }
}

case class KeyValueReply() extends Bundle {
  val id = LegoMemKV.Id()
  val success = Bool
}

case class KeyValueReplyInterface() extends Bundle with IMasterSlave {
  val header = Stream (KeyValueReply())
  val data   = Stream Fragment(Bits(512 bits))

  override def asMaster(): Unit = {
    master (header)
    master (data)
  }
}

