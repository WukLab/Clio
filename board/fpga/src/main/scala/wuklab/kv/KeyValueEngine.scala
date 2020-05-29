package wuklab.kv

import wuklab._
import spinal.core._
import spinal.lib._
import Utils._

// Process Bucket
class BucketProcessor(implicit config : KeyValueConfig) extends Component {
  val io = new Bundle {
    // Three FIFOs here:
    val bucketIn = slave (KeyValueBucketCommandInterface(useBucket = true))
    val entryOut = master (KeyValueEntryCommandInterface())

    // Commit to DMA
    val dmas = new Bundle {
      val nextLinkCommitPort = master (AxiStreamDMAWriteInterface(config.dmaConfig))
    }

    val alloc = master (config.allocInterfaceType)
  }

  val (matchDataF, forwardData) = StreamFork2(io.bucketIn.data)
  val matchData = matchDataF.takeFirst.queueLowLatency(4)

  // We will omit GC, for now, so we do not need to change the prev bucket
  // If True, we map this as a end signal
  // Match
  val matchCtrl = new Area {
    // We do a match here
    val numHtePerLine = 7
    val bucketsBits = io.bucketIn.bucket.fragment(447 downto 0)
    val linkBits = io.bucketIn.bucket.fragment(511 downto 448)

    // For multiple stage command
    val initialMask = Bits(numHtePerLine bits).setAll()
    // We commit the first bucket with greater than the current bucket
    val currentMask = Reg (Bits(numHtePerLine bit)) init initialMask

    val hteVec = bucketsBits.subdivideIn(64 bits).map { KeyValueHashEntry().fromFull(_) }
    val matchBits = Vec(
      hteVec.map(matchHashTableEntry(io.bucketIn.header.payload, matchData.payload.fragment, _))
    ).asBits

    val validBits = matchBits & currentMask
    val selected = unique(validBits)
    val selectedBin = OHToUInt(selected)
    val selectedEntry = hteVec(selectedBin)

    // Signal
    val valid = io.bucketIn.bucket.valid && io.bucketIn.header.valid
    val hit = validBits.orR
    val last = selected === validBits
    val commit = Bool

    // This means if we commit, we are committing the last valid entry in this line
    // If we finish the current bucket, we move forward
    // We need to pop bucket, ready, header
    matchData.ready := commit && last
    io.bucketIn.bucket.ready := commit && last
    io.bucketIn.header.ready := commit && last
    when (commit) {
      // move to next valid entry
      when (last) {
        currentMask := initialMask
      } otherwise {
        currentMask := initialMask |<< (selectedBin + 1)
      }
    }

    // 2. Info for insert
    // We also need to find a free slot, for write
    val freeBits = Vec(hteVec.map(!_.flags.valid)).asBits
    val uniqueFreeBits = unique(freeBits)
    val freeSlotBin = OHToUInt(unique(freeBits))
    val hasSlot  = freeBits.orR
    // 8 bytes per bucket
    val freeSlotNum = Mux(io.bucketIn.header.cmd === LegoMemKV.bucketCommand.WRITE, freeSlotBin, selectedBin)

    // 3. Info for the link
    val linkEntry = KeyValueHashEntry().fromFull(linkBits)
    val hasNext = linkEntry.flags.valid

  }

  // extra info
  // Will evaluate the commit phase, and decide on the commit
  val commitCtrl = new Area {

    // If theres any entry command committed
    val entryCommand = cloneOf(io.entryOut.header)
    val committed = Reg (Bool) init False

    // Data: being controlled imm
    // cmd: wait for the last, delay one request

    // Build the commit Stream. we can build multiple stage requests

    // The only output : 1. entry out
    matchCtrl.commit := entryCommand.fire
    entryCommand.valid := matchCtrl.valid && (matchCtrl.hit || matchCtrl.last)

    // Generate the command
    // Only fetch next will cause a loopback
    // Fill in the entry command
    entryCommand.last := matchCtrl.last
    // True for all writes
    entryCommand.matched := False
    entryCommand.hasNext := matchCtrl.hasNext
    entryCommand.entryValid := matchCtrl.hasSlot

    entryCommand.entry := matchCtrl.selectedEntry
    entryCommand.entrySlot := matchCtrl.freeSlotNum.resized

    val bucket = io.bucketIn.header.payload
    entryCommand.cmd := bucket.cmd
    entryCommand.id := bucket.id
    entryCommand.entryAddr := bucket.entryAddr
    entryCommand.linkAddr := matchCtrl.linkEntry.ptr
    entryCommand.allowOverride
    entryCommand.entry.keySize := bucket.keySize
    entryCommand.entry.valueSize := bucket.valueSize
    entryCommand.entry.digest(matchData.fragment)
    entryCommand.entry.flags.valid := matchCtrl.selectedEntry.flags.valid && matchCtrl.hit
    // This is for write back
    when (entryCommand.is(LegoMemKV.entryCommand.WRITE)) {
      entryCommand.entry.flags.valid := True
    }

  }

  // Will analyze the output stream, and try to find a slot
  // ALLOC_AND_INSERT_FST
  val hteAllocCtrl = new Area {
    // filter the requests and try to do insert
    // If need alloc, file a request
    // Start from commit fifo
    // TODO: consider the FIFO here
    val (commitHeader, allocHeader) = StreamFork2(commitCtrl.entryCommand.queue(16))

    // If its a single allocation, we do a single
    // Else we do a double allocation
    io.alloc.request.translateFrom (
      allocHeader.takeBy { c => c.is(LegoMemKV.entryCommand.WRITE) && !c.loopback }
    ) { (alloc, cmd) =>
      alloc.size := cmd.kvSize
      alloc.cmd := Mux(cmd.entryValid, LegoMemKV.ctrlCommand.ALLOC_KV_ONLY, LegoMemKV.ctrlCommand.ALLOC_KV_AND_HTE)
    }

    // If need alloc HTE, fetch from FIFO0,
    // The condition should be the
    val afterBucketAlloc = commitHeader
      .mergeBy (
        b => b.is(LegoMemKV.entryCommand.WRITE) && !b.entryValid && !b.loopback,
        io.alloc.response(0)
      ) { (entryCmd, addr) =>
        // TODO: once we alloc, we swap the entry and link for this write, since its solved
        entryCmd.linkAddr := addr
      }

    val (toKVAlloc, toLinkCommit) = StreamFork2(afterBucketAlloc.stage())

    // If need alloc KV, fetch from FIFO1
    val afterKVAlloc = toKVAlloc
      .mergeBy(
        c => c.is(LegoMemKV.entryCommand.WRITE) && !c.loopback,
        io.alloc.response(1)
      ) { (c, addr) =>
        // When we allocated a new bucket, we swap
        when (!c.entryValid && !c.hasNext) {
          c.entryAddr := c.linkAddr
        }
        c.matched := True
        c.entry.ptr := addr
      }

    // Commit to the linked position, we judge by if we have allocated the command
    val nextLinkStream = toLinkCommit.takeBy { b =>
      b.is(LegoMemKV.entryCommand.WRITE) && !b.entryValid && !b.hasNext
    }
    val (nextLinkCmd, nextLinkData) = StreamFork2(nextLinkStream)
    io.dmas.nextLinkCommitPort.cmd.translateFrom (nextLinkCmd) { (dma, bucket) =>
      // The link offset
      dma.addr := bucket.entryAddr + 56
      // 8 Bytes
      dma.len := 8
    }
    io.dmas.nextLinkCommitPort.data.translateFrom (nextLinkData) { (frag, bucket) =>
      val entry = KeyValueHashEntry()
      // The next entry
      entry.ptr := bucket.linkAddr
      entry.flags.valid := True

      // This is an empty entry, we do not need this field
      entry.keySize := 0
      entry.valueSize := 0
      entry.check := 0

      frag.last := True
      frag.fragment := entry.asFull.resized
    }

  }

  io.entryOut.data << forwardData.queue(config.dmaLatency)
  io.entryOut.header << hteAllocCtrl.afterKVAlloc

  // We have a command fifo, and controlled by a queue
  // We can emit this signal at half or at donw.
  def matchHashTableEntry(cmd : KeyValueBucketCommand, key : Bits, entry : KeyValueHashEntry) : Bool = {
    val isMatch = entry.flags.valid && (key(7 downto 0) === entry.check)
    // If is a match, we will find a empty slot, nothing is matched; else go the normal process
    Mux(cmd.cmd === LegoMemKV.bucketCommand.WRITE, False, isMatch)
  }
}

// READ: fetch AND read
// WRITE: write, with allocation path
// DELETE:

class EntryProcessor(implicit config : KeyValueConfig) extends Component {
  val io = new Bundle {
    val req = slave (KeyValueEntryCommandInterface())
    val res = master (KeyValueReplyInterface())

    // Loop back when no match
    val loopback = master (KeyValueBucketCommandInterface())

    // Commit to DMA
    val dmas = new Bundle {
      val dataFetchPort = master (AxiStreamDMAReadInterface(config.dmaConfig))
      val dataCommitPort = master (AxiStreamDMAWriteInterface(config.dmaConfig))
      val pointerCommitPort = master (AxiStreamDMAWriteInterface(config.dmaConfig))
    }
  }

  // delay data
  // Queues:
  //  1. data commit (write, update)
  //  2. pointer commit (write, delete) (no free for now)
  // Fork Reply or no reply (merged into one stream)
  //  3. reply (non-loopback)
  //  4. loopback (loopback)

  // Requests
  // 1. First Stage, commit if we need
  import LegoMemKV.entryCommand._

  val fetchCtrl = new Area {
    val (commitCmdF, cmdF) = StreamFork2(io.req.header)
    // Commit when we need match
    val (dmaCmd, sizeCmd) = StreamFork2(commitCmdF.takeBy { c => c.needMatch })
    io.dmas.dataFetchPort.cmd.translateFrom (dmaCmd) { (dma, cmd) =>
      dma.addr := cmd.entry.ptr
      dma.len := cmd.fetchSize
    }

    val parser = new FixedHeaderParser(512, 16, 64)
    parser.io.dataIn << io.dmas.dataFetchPort.data
    parser.io.size << sizeCmd.fmap(_.fetchSize).queue(config.dmaLatency)

    // Add a small queue to let data go
    val fetchedKey = parser.io.headerOut.queueLowLatency(2)
    val fetchedData = parser.io.dataOut

    val delayedData = io.req.data.queue(log2Up(config.dmaLatency))
    val delayedCmd = cmdF.queue(log2Up(config.dmaLatency))
  }

  // 2. If commit finish, then go 2nd stage, the lookup (&match) (we add a match tag at the key) (merge when)
  // Since we only have little data, we should fetch data with the key
  val matchCtrl = new Area {
    // 16Multiple fetched match one delayed
    val (delayedKeyF, delayedDataF) = StreamFork2(fetchCtrl.delayedData)
    val delayedKey = delayedKeyF
      .takeFirst
      .fmap(_.fragment(config.keyWidth-1 downto 0))
      .queueLowLatency(4)

    // First match
    val matchedCmdF = fetchCtrl.delayedCmd
      // Block when we need match and cannot match
      .continueWhen(delayedKey.valid && (!fetchCtrl.delayedCmd.needMatch || fetchCtrl.fetchedKey.valid))
      .changeBy { c =>
        val matched = keyMatch(fetchCtrl.fetchedKey.payload, delayedKey.payload, c.entry.keySize)
        when (c.needMatch) { c.matched := matched }
      }
    val matchedCmdFStaged = matchedCmdF.queue(16)
    // This signal need to to fire for every 'need match' data
    fetchCtrl.fetchedKey.ready := matchedCmdF.fire && matchedCmdF.needMatch
    // Every request will have this delayed key, so pop when last
    delayedKey.ready := matchedCmdF.fire && matchedCmdF.last

    // TODO: consider this stage?
    val Seq(matchedCmdP, matchSignalF, hitSignalF) = StreamFork(matchedCmdFStaged, 3)

    // Fork 0.
    // Continue the stream
    // Generate exact ONE Command for one line
    val cmdCommitted = Reg (Bool) init False
    val matchedCmd  = matchedCmdP.takeBy { c =>
      c.matched || (c.last && !cmdCommitted)
    }
    when (matchedCmdP.fire && matchedCmdP.last) {
      cmdCommitted := False
    } elsewhen (matchedCmd.fire) {
      cmdCommitted := True
    }

    // Fork 1.
    // Pop singal for line input data. Generate exact one for one line
    // Matched Signal, show if to last is fetched. used on delayed use a state machine for this
    // rule out invalid data
    val dataCommitted = Reg (Bool) init False
    val keepInputDataSignal = matchSignalF
      // Commit for every last, every line have only of this signal, every line have to commit one!
      // dataCommitted is only for early release
      .takeBy { c => c.matched || (c.last && !dataCommitted) }
      // Commit when 1. is WRITE, write can not fail
      // 2. is UDPATe and FIND the KEY, or loopback
      // 1 & 2 will consume the data
      // 3. can LOOPBACK and no match
      // About the last: if its valid, we send it to its place; else we send it to loopback
      // We use loopback here, it will be rule out the failed request
      .fmap { c => c.is(WRITE) || (c.is(UPDATE) && c.matched) || (c.loopback && !c.matched) }
    when (matchSignalF.fire && matchSignalF.last) {
      dataCommitted := False
    } elsewhen (keepInputDataSignal.fire) {
      dataCommitted := True
    }

    // Fork 2.
    // Hit Signal, show if fetch data is used. used on fetched data
    val hitSignal = hitSignalF.takeBy(_.needFetchedData).fmap(_.matched)

    // Data pathes
    val fetchedData = fetchCtrl.fetchedData.queue(16).filterBySignal(hitSignal)
    val delayedData = delayedDataF.filterBySignal(keepInputDataSignal.queueLowLatency(4))
  }

  // 3. Fork command
  // 3.1 Fork 1, filter (write) data commit (write)
  // 3.2 Fork 2, filter (write, delete)
  //  3.2.1 Fork 1, filter (write), datacommit
  //  3.2.2 Fork 2, filter (write, delete)
  // 3.3 Fork 3, all, filter last
  //  3.3.1 reply
  //  3.3.2 loopback

  // Loopback : last, but not matched
  // Fork Data:
  // All loopback will need data
  // all (reply & loopback) and (write, update) will need data

  // We prepared all data in the match phase, now only commit is enough
  val outputCtrl = new Area {
    // ==== FIFOs =====
    // input command: single line, single command
    // Command Path
    val Seq(dataCommitF, metaCommitF, replyLoopbackCommit, dataSignalF) = StreamFork(matchCtrl.matchedCmd, 4)
    // 3.1 Fork 1, filter (write, update) data commit (write) Commit to data
    val dataCommit = dataCommitF.stage().takeBy { c => c.is(WRITE, UPDATE) && c.matched }
    //  3.2.2 Fork 2, filter (write, delete) commit to pointer port
    val metaCommit = metaCommitF.stage().takeBy { c => c.is(WRITE, DELETE) && c.matched }

    val Seq(replyCommit, loopbackCommit) = StreamDemux(
      replyLoopbackCommit,
      (!replyLoopbackCommit.matched && replyLoopbackCommit.hasNext).asUInt,
      2)

    // Data Path, delayed data goes to
    // TODO: merge with prev judge
    val dataSignal = dataSignalF
      .takeBy { c => c.is(WRITE) || (c.is(UPDATE) && c.matched) || (c.loopback && !c.matched) }
      .fmap { c => (c.is(WRITE) || c.is(UPDATE)) && c.matched }
      .queueLowLatency(4)
    val (commitData, loopbackData) = matchCtrl.delayedData.forkBySignal(dataSignal)

    // ===== Outputs ======
    // 1. data commit port
    io.dmas.dataCommitPort.cmd.translateFrom (dataCommit) { (dma, entry) =>
      dma.len := entry.kvSize
      dma.addr := entry.entry.ptr
    }
    io.dmas.dataCommitPort.data <-< commitData

    // 2. meta commit port
    val (metaCommand, metaData) = StreamFork2(metaCommit)
    io.dmas.pointerCommitPort.cmd.translateFrom (metaCommand) { (dma, entry) =>
      dma.len := entry.entry.fullBytes
      dma.addr := entry.entrySlotAddr
    }
    io.dmas.pointerCommitPort.data.translateFrom (metaData) { (frag, entry) =>
      frag.last := True
      frag.fragment := entry.entry.asFull.resized
    }

    // 3. reply
    io.res.header.translateFrom (replyCommit) { (rep, cmd) =>
      rep.id := cmd.id
      rep.success := cmd.matched
    }
    io.res.data <-< matchCtrl.fetchedData

    // 4. loopback
    io.loopback.data <-< loopbackData
    io.loopback.header.translateFrom (loopbackCommit.stage()) { (bucket, entry) =>
      // TODO: fill in this
      bucket.cmd := entry.cmd
      bucket.keySize := entry.entry.keySize
      bucket.valueSize := entry.entry.valueSize
      bucket.id := entry.id
      bucket.entryAddr := entry.linkAddr
    }
  }

  def keyMatch(key: Bits, data: Bits, size: UInt) : Bool =
    key(config.keyWidth-1 downto 0) === data(config.keyWidth-1 downto 0)
}

// This module will include some of the most import things
class KeyValueExecutionUnit(implicit config: KeyValueConfig) extends Component {

  val io = new Bundle {
    val req = slave (KeyValueBucketCommandInterface(useBucket = true))
    val loopback = master (KeyValueBucketCommandInterface())
    val res = master (KeyValueReplyInterface())

    // we do 1st stage dma wrap/arbit at here!
    val dma = master (AxiStreamDMAInterface(config.dmaConfig))
    val allocs = master (config.allocInterfaceType)
  }

  val bucket = new BucketProcessor
  val entry = new EntryProcessor

  bucket.io.bucketIn <> io.req
  bucket.io.entryOut <> entry.io.req
  bucket.io.alloc <> io.allocs

  entry.io.res <> io.res
  entry.io.loopback <> io.loopback

  // Fst stage arbitration here
  val writeArbiter = new AxiStreamDMAWriteArbiter(3, config.dmaConfig)
  writeArbiter.io.writes(0) <> bucket.io.dmas.nextLinkCommitPort
  writeArbiter.io.writes(1) <> entry.io.dmas.dataCommitPort
  writeArbiter.io.writes(2) <> entry.io.dmas.pointerCommitPort

  io.dma.write <> writeArbiter.io.dma
  io.dma.read <> entry.io.dmas.dataFetchPort
}

class KeyValueStore(implicit config : KeyValueConfig) extends Component {

  type HashFunction = Lookup3HashFunction

  val numBlocks = 128
  val blockWidth = log2Up(numBlocks)
  val hashResultWidth = config.hteEntryWidth

  val io = new Bundle {
    val lego = new Bundle {
      val headerIn  = slave Stream KeyValueHeader()
      val dataIn    = slave Stream Fragment(Bits(512 bits))

      val headerOut  = master Stream KeyValueReplyHeader()
      val dataOut    = master Stream Fragment(Bits(512 bits))
    }

    val issue = new Bundle {
      val bucketOut = master (KeyValueBucketCommandInterface())
      val replyIn = slave (KeyValueReplyInterface())
    }
  }

  val ids = new IDPool(numBlocks)
  val mem = new Mem(KeyValueHeaderStorage.compactType, numBlocks)

  val (dataStream, keyStreamF) = StreamFork2(io.lego.dataIn)
  val keyStream = keyStreamF.takeFirst.fmap(_.fragment(63 downto 0))

  // 0. do the hash
  val inputCtrl = new Area {

    // Pipeline function
    val hashFifo = StreamFifoLowLatency(UInt(hashResultWidth bits), 8)
    val hashFunc = new HashFunction(0, hashResultWidth, hashFifo.io.push.ready)

    val hashValue = hashFunc.hash(keyStream.payload.asUInt)
    hashFifo.io.push.valid := Delay(keyStream.valid, hashFunc.pipelineDelay, hashFifo.io.push.ready, False)
    hashFifo.io.push.payload := hashValue
    keyStream.ready := hashFifo.io.push.ready

    // The stream
    val delayedHeader = io.lego.headerIn.queueLowLatency(hashFunc.pipelineDelay)
    val (allocatedHeader, forwardHeaderF) = StreamFork2(delayedHeader >*< ids.io.alloc)

    // 2. we save states
    // Submit request to memory
    mem.writePort.translateFrom (allocatedHeader.toFlow) { (cmd, p) =>
      cmd.address := p.snd
      cmd.data := KeyValueHeaderStorage.compress(p.fst)
    }

    // 3. we forward the request
    val forwardHeader = forwardHeaderF >*< hashFifo.io.pop
    // TODO: check here
    io.issue.bucketOut.data << dataStream.queueLowLatency(4)
    io.issue.bucketOut.header.translateFrom (forwardHeader) { (bucket, p) =>
      val header = p.fst.fst

      bucket.entryAddr := config.hashToAddress(p.snd)
      bucket.id := p.fst.snd.resized
      bucket.cmd := LegoMemKV.bucketCommand.fromRequestCode(header.header.reqType)
      bucket.keySize := header.keySize.resized
      bucket.valueSize := header.valueSize
    }
  }

  val outputCtrl = new Area {
    val Seq(readCmd, replyCmdF, freeCmd) = StreamFork(io.issue.replyIn.header, 3)
    val replyCmd = replyCmdF.queueLowLatency(4)

    // Read data out
    val bitStream = mem.streamReadSync (readCmd.fmap(_.id.resize(blockWidth)))
    io.lego.headerOut.translateFrom (bitStream.fmap(KeyValueHeaderStorage.decompress) >*< replyCmd) { (rep, p) =>
      val repHeader = p.fst
      val repKV = p.snd

      // TODO: fill in the legomem REPL
      rep := repHeader
      rep.allowOverride
      rep.header.reqType := repHeader.header.reqType + 1
      rep.header.reqStatus := Mux(repKV.success, LegoMem.RequestStatus.OKAY, LegoMem.RequestStatus.ERR_INVALID)
      rep.header.size := Mux(
        repHeader.header.reqType === LegoMem.RequestType.KV_READ && repKV.success,
        repHeader.packedBytes + repHeader.valueSize,
        U(repHeader.packedBytes, 16 bits)
      )
      rep.header.cont := LegoMem.Continuation(LegoMem.Continuation.EP_NETWORK)
    }
    io.lego.dataOut << io.issue.replyIn.data
    ids.io.free << freeCmd.toFlow.fmap(_.id.resize(blockWidth))
  }
}

// Hash and push forward
//class KeyValueLock(implicit config : KeyValueConfig) extends Component {
//
//  val numLockBlocks = 128
//  val lockBlockWidth = log2Up(numLockBlocks)
//  val hashResultWidth = 16
//  type HashFunction = Lookup3HashFunction
//  val enableLock = false
//
//  val io = new Bundle {
//    val lego = new Bundle {
//      val headerIn  = slave Stream KeyValueHeader()
//      val dataIn    = slave Stream Fragment(Bits(512 bits))
//
//      val headerOut  = master Stream KeyValueReplyHeader()
//      val dataOut    = master Stream Fragment(Bits(512 bits))
//    }
//
//    val issue = new Bundle {
//      val bucketOut = master (KeyValueBucketCommandInterface())
//      val replyIn = slave (KeyValueReplyInterface())
//    }
//  }
//
//  val ids = new IDPool(numLockBlocks)
//  val cam = new LookupTCamStoppable(hashResultWidth, log2Up(numLockBlocks), useEnable = true)
//  val hashFunc = new HashFunction(0, hashResultWidth, True)
//  val mem = new Mem(KeyValueHeaderStorage.compactType, numLockBlocks)
//
//  val (dataStream, keyStreamF) = StreamFork2(io.lego.dataIn)
//  val keyStream = keyStreamF.takeFirst.fmap(_.fragment)
//
//  // 0. do the hash
//  val inputCtrl = new Area {
//    // TODO: key? we need first data here
//    val hashResult = hashFunc.hash(keyStream.payload(63 downto 0).asUInt)
//    val delayedHeader = io.lego.headerIn.queueLowLatency(log2Up(hashFunc.pipelineDelay + cam.pipelineDelay))
//    val hashStream = ReturnStream(hashResult)
//
//    // TODO: add FIFOs around here
//    // TODO: I'm sure we will have problem here
//    // 1. We lock, every id will lock!
//    // We keep a tag <-> DMA table here
//    // Or, reorder buffer
//    val Seq(readHash, writeHash, forwardHash) = StreamFork(hashStream, 3)
//    cam.io.rd.req.translateFrom (readHash) { _.key := _ }
//
//    val releaseSignal = cam.io.rd.res.takeBy(_.hit).queueLowLatency(4)
//    val freeHeader = delayedHeader <* releaseSignal
//    keyStream.ready := releaseSignal.fire
//
//    val (saveId, lockId) = StreamFork2(ids.io.alloc)
//
//    val (allocatedHeader, forwardHeaderF) = StreamFork2(freeHeader >*< saveId)
//    val insertStream = lockId >*< writeHash
//
//    // If we are doing a reorder buffer, we can do a head&tail
//    // else, we do a ID Pool
//
//    // 2. we save states
//    // Submit request to Home
//    mem.writePort.translateFrom (allocatedHeader.toFlow) { (cmd, p) =>
//      cmd.address := p.snd
//      cmd.data := KeyValueHeaderStorage.compress(p.fst)
//    }
//
//    // 3. we forward the request
//    val forwardHeader = forwardHeaderF >*< forwardHash
//    // TODO: check here
//    io.issue.bucketOut.data << dataStream.queueLowLatency(4)
//    io.issue.bucketOut.header.translateFrom (forwardHeader) { (bucket, p) =>
//      val header = p.fst.fst
//
//      bucket.entryAddr := config.hashToAddress(p.snd)
//      bucket.id := p.fst.snd.resized
//      bucket.cmd := LegoMemKV.bucketCommand.fromRequestCode(header.header.reqType)
//      bucket.keySize := header.keySize.resized
//      bucket.valueSize := header.valueSize
//    }
//  }
//
//  val outputCtrl = new Area {
//    val Seq(readCmd, replyCmdF, releaseCmd, freeCmd) = StreamFork(io.issue.replyIn.header, 4)
//    val replyCmd = replyCmdF.queueLowLatency(4)
//
//    // Read data out
//    val bitStream = mem.streamReadSync (readCmd.fmap(_.id.resize(lockBlockWidth)))
//    io.lego.headerOut.translateFrom (bitStream.fmap(KeyValueHeaderStorage.decompress) >*< replyCmd) { (rep, p) =>
//      val repHeader = p.fst
//      val repKV = p.snd
//
//      // TODO: fill in the legomem REPL
//      rep := repHeader
//      rep.allowOverride
//      rep.header.reqType := repHeader.header.reqType + 1
//      rep.header.size := Mux(
//        repHeader.header.reqType === LegoMem.RequestType.KV_READ,
//        repHeader.header.packedBytes + repHeader.valueSize,
//        U(repHeader.header.packedBytes, 16 bits)
//      )
//      rep.header.reqStatus := LegoMem.RequestStatus.OKAY
//      rep.header.cont := LegoMem.Continuation(LegoMem.Continuation.EP_NETWORK)
//    }
//    io.lego.dataOut << io.issue.replyIn.data
//    ids.io.free << freeCmd.toFlow.fmap(_.id.resize(lockBlockWidth))
//  }
//
//  // End Queue
//  val camCtrl = new Area {
//    val insert = Stream(cam.io.wr.payloadType).translateFrom (inputCtrl.insertStream) { (cmd, lock) =>
//      cmd.enable := True
//      // TODO: PLEASE CHECK THIS
//      cmd.mask.setAll()
//      cmd.key := lock.snd
//      cmd.value := lock.fst
//    }
//
//    val free = Stream(cam.io.wr.payloadType).translateFrom (outputCtrl.releaseCmd) { (cmd, release) =>
//      cmd.enable := False
//      cmd.mask.setAll()
//      cmd.key := 0
//      cmd.value := release.id.resized
//    }
//
//    cam.io.wr << StreamArbiterFactory.lowerFirst.onArgs(free, insert).toFlow
//  }
//
//  // For now, we do not do reorder, just send it here
//}

// Fetch HTE and move on
class KeyValueIssuer(implicit config : KeyValueConfig) extends Component {
  val io = new Bundle {
    // From lock & loopback, 2 input streams
    val request     = slave (KeyValueBucketCommandInterface())
    val loopback    = slave (KeyValueBucketCommandInterface())
    val issue       = master (KeyValueBucketCommandInterface(useBucket = true))

    val dma         = master (AxiStreamDMAReadInterface(config.dmaConfig))
  }

  // We need something
  // Commit to DDR, and fetch back
  // Merge the requests here
  val (mergedHeader, mergedData) = StreamWithFragmentArbiter
    .onInterface(io.loopback, io.request) { ifc => (ifc.header, ifc.data) }

  val (commit, continue) = StreamFork2(mergedHeader)
  io.dma.cmd.translateFrom (commit) { (dma, bucket) =>
    dma.addr := bucket.entryAddr
    dma.len := 64
  }

  // Also need buffer for buckets, since issue may take multiple cycles
  io.issue.bucket << io.dma.data.queue(32)
  io.issue.header << continue.queue(config.dmaLatency)
  io.issue.data   << mergedData.queue(config.dmaLatency)
}
