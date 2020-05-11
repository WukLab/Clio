package wuklab.kv

import wuklab.{kv, _}
import spinal.core._
import spinal.lib._
import Utils._
import spinal.core.internals.Operator
import spinal.lib


//class KeyValueEndPointWithPhysicalMemory extends Component {
//  // Convert the DMA interface to the Physcial interfaece
//
//}
//
//class KeyValueEngine(implicit config : KeyValueConfig) extends Component {
//  // One -> multiple
//  // Queue for multiple instances
//  //
//
//  // Fetch the first bucket
//  // Fetch the second bucket
//  val io = {
//    val ep = LegoMemEndPoint()
//    val dma = AxiStreamDMAInterface()
//  }
//
//  // connect the modules
//
//  // do arbitration on modules
//
//}
//
//// This is a object view
//trait View[ViewT <: Data, T <: Data] {
//  implicit def toView   (t : T) : ViewT
//  implicit def fromView (t : ViewT) : T
//  implicit def assignToView   (view : ViewT, t : T) : Unit = view := toView(t)
//  implicit def assignFromView (t : T, view : ViewT) : Unit = t := fromView(view)
//}
//
//// 64 bit size
//// 32 addr + flags, 16 version + 4 bit 2nd hash (last 4 bit)
//
//// match function: given a KV Command & a PTE, print out a match
//
//// Page Table Structure: |ptr|bitmap + version map + size + version|secondary hash|linked|next
//
//// Get Hash    -> FireDMA Request -> Process Engine -> Insert (fetch PTE, KV, ll) (WB Cache?) -> PTE Commit
////             -> wait  kv FIFO   /                                                           -> Data Commit
////             -> wait cmd FIFO  /
//
//// DMA Engine
//// Virtual channel
//case class KeyValueHashEntry() extends Bundle {
//  val ptr     = UInt(28 bits)
//  val flags = new Bundle {
//    val valid   = Bool
//  }
//  val valueSize = UInt(16 bits)
//  val keySize   = UInt(4 bits)
//  val blockSize = UInt(4 bits)
//  val check     = UInt(8 bits)
//
//  def physicalPtr(base : UInt) : UInt = base + (ptr << 4).resize(widthOf(base))
//
//  override def asBits : Bits = {
//    val bits = Bits(64 bits)
//    bits(0)             := flags.valid
//    bits(31 downto 4)   := ptr      .asBits
//    bits(47 downto 32)  := valueSize.asBits
//    bits(51 downto 48)  := keySize  .asBits
//    bits(55 downto 52)  := blockSize.asBits
//    bits(63 downto 56)  := check    .asBits
//    bits
//  }
//
//  override def assignFromBits(bits: Bits): Unit = {
//    assert(widthOf(bits) == 64, "Width mismatch of the valid")
//    flags.valid := bits(0)
//    ptr       := bits(31 downto 4) .asUInt
//    valueSize := bits(47 downto 32).asUInt
//    keySize   := bits(51 downto 48).asUInt
//    blockSize := bits(55 downto 52).asUInt
//    check     := bits(63 downto 56).asUInt
//  }
//
//
//}
//
//object LegoMemKV {
//  object bucketCommand {
//    def apply() = UInt(8 bits)
//    def READ   = U"8'h0"
//    def WRITE  = U"8'h1"
//    def UPDATE = U"8'h2"
//    def DELETE = U"8'h3"
//  }
//  object entryCommand {
//    def apply() = UInt(8 bits)
//    def WRITE = U"8'h1"
//
//    // internal Use
//    def FETCH_READ   = U"8'h80"
//    def FETCH_UPDATE = U"8'h81"
//    def FETCH_DELETE = U"8'h82"
//
//    // ctrl
//    def needFetch(cmd : UInt): Unit = cmd(7)
//  }
//  object Size {
//    def apply() = UInt(4 bits)
//    def SIZE16  = U"4'h0"
//    def SIZE32  = U"4'h1"
//    def SIZE64  = U"4'h2"
//    def SIZE128 = U"4'h3"
//    def SIZE256 = U"4'h4"
//    def SIZE512 = U"4'h5"
//  }
//  object Id {
//    def apply() = UInt(16 bits)
//  }
//}
//
//// Lock and unlock
//
//// Commands:
////  PROCESS
////    READ
////    WRITE
////  CHECK AND INSTALL
////    CHECK AND READ
////    CHECK AND UPDATE
////    CHECK AND RETURN
////  CHECK AND SEND?
//
//// versioned WRITE -> WIRTE, CHECK AND UPDATE
//// versioned READ -> READ, SELECT TOP VERSION, AND UPDATE
//
//// PUT? match -> insert
//
//// We must lock on PTE
//// We give all PTE Writes
//case class KeyValueBucketCommand() extends Bundle {
//  // keysize < 512
//
//  val cmd       = LegoMemKV.bucketCommand()
//  val keySize   = LegoMemKV.Size()
//  val valueSize = UInt(16 bits)
//  val reqId     = LegoMemKV.Id()
//  // This is for PTE Addr. The last bit is for valid
//  // This is for write back
//  val hash      = UInt(32 bits)
//}
//
//case class KeyValueEntryCommand() extends Bundle {
//  val cmd       = LegoMemKV.entryCommand()
//  val keySize   = LegoMemKV.Size()
//  val valueSize = LegoMemKV.Size()
//  val last      = Bool
//
//  val tag       = UInt(16 bits) // for lock
//  val reqId     = UInt(16 bits)
//  val entry     = KeyValueHashEntry()
//}
//
//case class KeyValueReply() extends Bundle {
//  val id = LegoMemKV.Id()
//}
//
//// If it is a dma read
//// DO_READ
//// TRY_READ
//
//// if its a dma write
//// DO_WRITE
//
//// if its a dma delete -> Try TO GC PTE, return,
//// TRY_DELETE
//// DO_DELETE
//// TRY_DELETE AND GC
//// DO_DELETE AND GC
//
//
//// Fetch?
//// Not?
//// Process Per entry operation
//class EntryProcessor() extends Component {
//  val io = new Bundle {
//    val cmd = slave Stream KeyValueEntryCommand()
//    val data = slave Stream Fragment(Bits(512 bits))
//    val repl = master Stream KeyValueReply()
//    val blockReport = master Stream KeyValueReply()
//
//    //
//    val asyncFifos = Vec(slave Stream UInt(32 bits), 4)
//
//    // Commit to DMA
//    val dma = master (AxiStreamDMAInterface())
//  }
//
//  val dmaLatency = 64
//
//  // Commit queues
//  val dataCommitPort = AxiStreamDMAWriteInterface()
//  val entryCommitPort = AxiStreamDMAWriteInterface()
//
//  // Commit things that need fetch back
//  val cmdF = io.cmd
//  val (commit, cmd) = StreamFork2(cmdF)
//
//  val fetchCtrl = new Area {
//    io.dma.read.cmd.translateFrom (commit.continueBy(needFetch)) { (dma, kv) =>
//
//    }
//
//    val keyStream = io.dma.read.data.takeFirst
//  }
//
//  val cmdP = cmd.queue(log2Up(dmaLatency))
//  val dataP = io.data.queue(log2Up(dmaLatency))
//  val dataSignal = Stream(Bool).queueLowLatency(8)
//
//  // delay data
//  val matchCtrl = new Area {
//    // First, we do a
//    // TODO: both these FIFOs needs some cache
//    val (inputKeyF, inputPairF) = StreamFork2(dataP)
//    val inputKeyStream = inputKeyF.takeFirst.fmap(_.fragment)
//    val fetchedKeyStream = fetchCtrl.keyStream.fmap(_.fragment)
//
//    // CMD Fork, redo the query
//    val cmdStream = cmdP
//    val replyStream = io.repl
//    val reportStream = io.blockReport
//
//    // Statemachine
//    val requireCommit = Reg (Bool) init False
//    val dataCommitted = Reg (Bool) init False
//    val currentIdReg  = Reg (LegoMemKV.Id()) init 0
//    val idChange = currentIdReg =/= cmdStream.reqId
//
//    // BIG control logic
//    // initialization of the ports
//    inputKeyStream.ready := False
//    cmdStream.ready := False
//
//    val curCmd = cmdStream.payload
//    def isValidMatch = inputKeyStream.valid && fetchedKeyStream.valid
//    def isLast = curCmd.last
//    def isValidLast = curCmd.last && (dataCommitted || dataSignal.ready)
//    def isKeyMatch = keyMatch(inputKeyStream, fetchedKeyStream, curCmd.keySize)
//
//    def readyToCommitReply = replyStream.ready
//    def commitReply (assign : KeyValueReply => Unit) = {
//      replyStream.valid := True
//    }
//    def readyToCommitReport = reportStream.ready
//    def readyToCommitData = readyToCommitReply && dataCommitPort.cmd.ready && dataSignal.ready
//    def commitData = {
//      dataSignal.valid := True
//      dataSignal.payload := True
//    }
//    def readyToCommitEntry = entryCommitPort.cmd.ready
//    def commitEntry (addr : UInt)(assign : KeyValueHashEntry => Unit) = {
//
//    }
//    def readyToCommitBoth = readyToCommitData && readyToCommitEntry
//
//    when (cmdStream.valid) {
//      // Control the input key fifo
//
//      // if Id is change and not committed, we do the false
//      when (dataCommitted) {
//        // We finish the job! just ignore other commands
//        switch (cmdStream.payload.cmd) {
//          is (
//            LegoMemKV.entryCommand.FETCH_READ,
//            LegoMemKV.entryCommand.FETCH_UPDATE,
//            LegoMemKV.entryCommand.FETCH_DELETE
//          ) {
//            // Read out the returned key
//            when (fetchedKeyStream.valid) {
//              fetchedKeyStream.ready := True
//              cmdStream.valid := True
//            }
//
//          }
//
//          is (LegoMemKV.entryCommand.WRITE) {
//            // Pop the cmd
//            cmdStream.ready := True
//          }
//
//        }
//
//      } otherwise {
//        // We execute the command
//        switch (cmdStream.payload.cmd) {
//          // READ
//          is (LegoMemKV.entryCommand.FETCH_READ) {
//            // requires: key
//            // generate: resp
//            when (isValidMatch) {
//              // If its a matched, we commit
//              when (isKeyMatch) {
//                // We need to commit the data
//                when (readyToCommitData) {
//                  // We pop all both inputKey and fetchKey
//
//                }
//              } elsewhen (isLast) {
//                // we just pop the cmd & fetched
//                when (isValidLast) {
//                  // we only pop inputKey
//
//                }
//              } otherwise {
//                // We pop everything
//
//              }
//            }
//          }
//          is (LegoMemKV.entryCommand.FETCH_DELETE) {
//            // require: key
//            // generate: entryPort, resp
//
//          }
//          is (LegoMemKV.entryCommand.FETCH_UPDATE) {
//            // require: key
//            // generate: entryPort, dataPort, resp
//
//          }
//          is (LegoMemKV.entryCommand.WRITE) {
//            // require: n/a
//            // generate: entryPort, dataPort, resp
//            when (readyToCommitBoth) {
//              // We pop all both inputKey and fetchKey
//              commitData
//              commitEntry(curCmd.)
//              commitReply {
//
//              }
//
//
//
//            }
//
//          }
//
//        }
//
//      }
//
//    }
//
//  }
//
//  // TODO: sometimes, we do not have cmd
//  // TODO: Use a fork here
//
//  // The data fifo is async
//  // then we do read/write
//  io.dma.write.data << dataP.filterBySignal(dataSignal)
//
//  def needFetch(cmd : KeyValueEntryCommand) : Bool
//  def needMatch(cmd : KeyValueEntryCommand) : Bool
//  def keyMatch(key: Stream[Bits], data: Stream[Bits], size: UInt) : Bool
//}
//
//// Process Bucket
//class BucketProcessor(matchFunction : (KeyValueBucketCommand, KeyValueHashEntry) => Bool) extends Component {
//  val numSyncFifos = 4
//  val io = new Bundle {
//    // Three FIFOs here:
//    val req = slave (KeyValueBucketCommandInterface())
//    val cmd = slave Stream KeyValueBucketCommand()
//    val bucket = slave Stream Fragment(Bits(512 bits))
//
//    // Commit FIFOs: data & command
//    val entryOut = new Bundle {
//      val cmd = master Stream KeyValueEntryCommand()
//      val data = slave Stream Fragment(Bits(512 bits))
//    }
//
//    val bucketOut = slave (KeyValueBucketCommandInterface())
//
//    // Async FIFOs
//    val asyncFifos = Vec(slave Stream UInt(32 bits), 4)
//
//    // Commit to DMA
//    val dma = master (AxiStreamDMAWriteInterface())
//  }
//
//  val numHtePerLine = 512 / 64
//  // Generate 1:n mapping
//  val (syncData, syncCmd) = io.bucket syncWith io.cmd
//
//  val prevLinkPort = new AxiStreamDMAWriteInterface()
//  val nextLinkPort = new AxiStreamDMAWriteInterface()
//
//  // If True, we map this as a end signal
//  val commandReleaseSignal = Stream(Bool)
//  val dataSignal = Stream(Bool)
//
//  // TODO: consider pipeline here
//  // TODO: This is a raw stage machine!
//  // TODO: the major problem: emit the last
//  val matchCtrl = new Area {
//    // For multiple stage command
//    val nextStage = UInt(4 bits)
//    val initialMask = Bits(32 bits).setAll()
//    // We commit the first bucket with greater than the current bucket
//    val currentMask = Reg (Bits(32 bit)) init initialMask
//
//    val committed = Reg (Bool) init False
//    // Data: being controlled imm
//    // cmd: wait for the last, delay one request
//
//    // We do a match here
//    val hteVec = io.bucket.fragment.as (Vec(KeyValueHashEntry(), numHtePerLine))
//    val matchBits = hteVec.map(matchFunction(syncCmd.payload, _)).reduce(_ ## _)
//
//    // TODO: if is last bucket, we try to move mask a little bit
//    val validBits = matchBits & currentMask
//    val selected = (validBits & (~validBits.asUInt + 1).asBits)
//    val selectedBin = OHToUInt(selected)
//    val selectedEntry = hteVec(selectedBin)
//
//    val valid = validBits.orR
//    val commit = valid
//
//    // Build the commit Stream. we can build multiple stage requests
//    val commitStream = Stream(KeyValueEntryCommand()).queue(???)
//
//
//    val goNextEntry = Mux(commit, commitStream.fire, True) && nextStage === 0
//    val goNextLine = selected === validBits
//    val goNextBucket = goNextLine && syncData.last
//    when (goNextEntry) {
//      currentMask := initialMask |<< selectedBin
//      syncData.ready := goNextLine
//    }
//
//    val linkCtrl = new Area {
//      // The first link channel
//
//    }
//
//    def commitCommand : Bool
//    def commitLastCommand : Bool
//
//    // About the release signal
//    // For the first command, we do not release
//    // For the other command, we issue a 0
//    // For the last command, we issue a 1
//
//    // ==== Core Logic
//    // The command generator
//    commitStream.valid := commit
//    commitStream.entry := selectedEntry
//    // Only fetch next will cause a loopback
//    when (valid) {
//      switch (syncCmd.cmd) {
//        is (LegoMemKV.bucketCommand.WRITE) {
//          // If we have a blank
//          // If we do not, but we are at end & have a linked list
//          // If we do not. but we are at end & do not have a linked list
//        }
//        is (LegoMemKV.bucketCommand.READ, LegoMemKV.bucketCommand.UPDATE, LegoMemKV.bucketCommand.DELETE) {
//          // If not the last & match
//          // If is last & linked
//          // if is last & not linked
//            // if is match
//            // if do not match
//          // If DELETE, Last, we issue GC
//          when (syncCmd.cmd === LegoMemKV.bucketCommand.DELETE) {
//
//          }
//        }
//      }
//    }
//
//
//
//  }
//
//  // We have a command fifo, and controlled by a queue
//  // We can emit this signal at half or at donw.
//
//  def assignEntryCommand(entry : KeyValueEntryCommand, bucket: KeyValueBucketCommand) : Unit
//
//  // Fork by if next
//  // Cmd
//  // Data
//}
//
//class KeyValueExecutionUnit() {
//
//  val io = new Bundle {
//    val req = slave (KeyValueBucketCommandInterface())
//    val loopback = master (KeyValueBucketCommandInterface())
//
//    val res = {
//
//    }
//
//    // we do 1st stage dma wrap/arbit at here!
//    val dmas = new Bundle {
//
//    }
//  }
//
//}
//
//class KeyValueLock extends Component {
//
//  type HashFunction = Lookup3HashFunction
//
//  val io = new Bundle {
//    val headerIn  = slave Stream KeyValueHeader()
//    val dataIn    = slave Stream Bits(512 bits)
//
//    val headerOut = master Stream KeyValueBucketCommand()
//    val dataOut   = master Stream Bits(512 bits)
//  }
//
//  val ids = new IDPool()
//  val cam = new LookupTCamStoppable()
//  val hashFunc = new HashFunction()
//
//  // 0. do the hash
//  val hashResult = hashFunc.hash(io.headerIn.payload |> getHashTag)
//  val delayedHeader = hashFunc streamDelay io.headerIn
//  val hashStream = ReturnStream(hashResult)
//
//  // 1. We lock
//  // We keep a tag <-> DMA table here
//  // Or, reorder buffer
//  val (readHash, writeHash) = StreamFork2(hashStream)
//  cam.io.rd.req.translateFrom (hashStream) { _.key := _ }
//
//  val releaseSignal = cam.io.rd.res.throwBy(_.hit).queueLowLatency(1)
//  val freeHeader = delayedHeader <* releaseSignal
//
//  val (saveHeader, forwardHeader) = StreamFork2(freeHeader)
//  val (saveId, lockId) = StreamFork2(ids.io.alloc)
//
//  val allocatedHeader = saveHeader >*< saveId
//  val Seq(saveHeaderStream, forwardHeaderStream, lockHeaderStream) = StreamFork(allocatedHeader, 3)
//
//  cam.io.wr.translateFrom (lockHeaderStream.toFlow) { (cmd, p) =>
//    cmd.key :=
//    cmd.value := p.snd
//  }
//
//  // If we are doing a reorder buffer, we can do a head&tail
//  // else, we do a ID Pool
//
//  // 2. we save states
//  // Submit request to Home
//  val mem = new Mem[]()
//  mem.writePort.translateFrom (saveHeaderStream.toFlow) { (cmd, p) =>
//    cmd.address := p.snd
//    cmd.data := ???
//  }
//
//  // End Queue
//
//  // For now, we do not do reorder, just send it here
//  io.dataOut << io.dataIn
//
//  def getHashTag(header: KeyValueHeader) : UInt
//}
//
//case class KeyValueBucketCommandInterface() extends IMasterSlave {
//  val header = slave Stream KeyValueBucketCommand()
//  val data   = slave Stream Fragment(Bits(512 bits))
//
//  override def asMaster(): Unit = {
//    master (header)
//    master (data)
//  }
//}
//
//class KeyValueIssuer extends Component {
//  val numBackEnd = 1
//  val io = new Bundle {
//    // From lock
//    val lockIn = slave (KeyValueBucketCommandInterface())
//    // loopback
//    val lookbackIn = slave (KeyValueBucketCommandInterface())
//
//    val issue = new Bundle {
//      val header = Vec(master Stream KeyValueBucketCommand(), numBackEnd)
//      val data   = Vec(master Stream Fragment(Bits(512 bits)), numBackEnd)
//    }
//
//    // dma ports
//    val dma = master (AxiStreamDMAReadInterface())
//  }
//
//  val dmaLatency = 128
//
//  // We need something
//  // Commit to DDR, and fetch back
//  // Merge the requests here
//
//  val (mergedHeader, mergedData) = StreamWithFragmentArbiter
//    .onInterface(io.lockIn, io.lookbackIn) { ifc => (ifc.header, ifc.data) }
//
//  val (commit, continueF) = StreamFork2(mergedHeader)
//  io.dma.cmd.translateFrom (commit) { (req, header) =>
//
//  }
//  val continue = continueF.queue(log2Up(dmaLatency))
//
//  // contains multiple issuer
//  // Current we only have one executor
//  val issueCtrl = new Bundle {
//    io.issue.header(0) << continue
//    io.issue.data(0)   << io.dma.data
//  }
//
//}
//
//// Good old arbtriting
//class KeyValueDMAController extends Component {
//  val numFullChannels = 4
//  val numReadChannels = 4
//  val numWriteChannels = 4
//
//  val numOutstandingReads = 256
//  // tag: channel ID
//  val io = new Bundle {
//    val fulls   = Vec(slave (AxiStreamDMAInterface()), numReadChannels)
//    val reads   = Vec(slave (AxiStreamDMAReadInterface()), numReadChannels)
//    val writes  = Vec(slave (AxiStreamDMAWriteInterface()), numWriteChannels)
//
//    val dma     = master (AxiStreamDMAInterface())
//  }
//
//  val readChannels = io.fulls.map(_.read) ++ io.reads
//  val writeChannels = io.fulls.map(_.write) ++ io.writes
//
//  // Read path
//  val readCtrl = new Area {
//    io.dma.read.data << StreamArbiterFactory.roundRobin.on(readChannels.map(_.cmd))
//
//    val fireVec = Vec(io.reads.map(_.cmd.fire))
//    val fireFifo = ReturnStream(fireVec, io.dma.read.data.fire).queue(log2Up(numOutstandingReads))
//
//    val readResults = StreamDemuxOH(io.dma.read.data, fireFifo.payload)
//    fireFifo.throwWhen(io.dma.read.data.lastFire)
//    (io.reads, readResults).zipped map (_.data << _)
//  }
//
//  // Write Path
//  val writeCtrl = new Area{
//    io.dma.read.data << StreamArbiterFactory.roundRobin.on(io.writes.map(_.cmd))
//
//    val fireVec = Vec(io.reads.map(_.cmd.fire))
//    val fireFifo = ReturnStream(fireVec, io.dma.read.data.fire).queueLowLatency(2)
//
//    val readResults = StreamDemuxOH(io.dma.read.data <* fireFifo, fireFifo.payload)
//    (io.reads, readResults).zipped map (_.data << _)
//  }
//
//}