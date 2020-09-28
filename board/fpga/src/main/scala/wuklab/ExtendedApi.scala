package wuklab

import spinal.core._
import spinal.lib._
import Utils._

class PointerChasingEndpoint(implicit config : CoreMemConfig) extends Component with XilinxAXI4Toplevel {
  // Convert the DMA interface to the LegoMem
  val io = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)

  val bridge = new RawInterfaceEndpoint
  bridge.io.ep <> io
  bridge.io.raw.ctrlIn.freeRun()
  bridge.io.raw.ctrlOut.disable

  import LegoMem.RequestType._
  val multiData = new LegoMemMultiDataInterface(Seq(_.is(POINTER_CHASING), !_.is(POINTER_CHASING)))
  multiData.io.in.dataIn << bridge.io.raw.dataIn
  multiData.io.in.dataOut >> bridge.io.raw.dataOut

  val engine = new PointerChasingEngine
  engine.io.pcEp <> multiData.io.out(0)
  engine.io.vmEp <> multiData.io.out(1)

  addPrePopTask(renameIO)
}


class PointerChasingEngine(implicit config : CoreMemConfig) extends Component {
  // One -> multiple
  // Queue for multiple instances
  //
  val io = new Bundle {
    val pcEp = slave (LegoMemRawDataInterface())
    val vmEp = slave (LegoMemRawDataInterface())
  }

  // Pointer chasing end point
  val requestQueue = io.pcEp.dataIn
    .takeFirst.toStream
    .fmap(PointerChasingHeader().fromWiderBits)
    .queue(32)

  val pcBuilder = new PacketBuilder(512, LegoMemAccessHeader(config.virtualAddrWidth))
  pcBuilder.io.dataOut >> io.pcEp.dataOut

  // VM end point
  val parser = new PacketParser(512, LegoMemAccessHeader(config.virtualAddrWidth))
  parser.io.dataIn << io.vmEp.dataIn

  val builder = new PacketBuilder(512, LegoMemAccessHeader(config.virtualAddrWidth))
  builder.io.dataOut >> io.vmEp.dataOut

  // arbitration of loopback and input
  val waitQueue = StreamFifo(PointerChasingHeader(), 64)
  val commitStream = StreamArbiterFactory.lowerFirst.onArgs(waitQueue.io.pop, requestQueue)

  // Commit and commit queue
  val (commitHeader, commitWaitF) = StreamFork2(commitStream)
  builder.io.dataIn.disable
  builder.io.headerIn.translateFrom (commitHeader) { (access, pc) =>
    access.allowOverride
    access.header := pc.header
    access.header.reqType := LegoMem.RequestType.READ
    access.header.reqStatus := LegoMem.RequestStatus.OKAY
    access.header.size := access.packedBytes
    import LegoMem.Continuation
    access.header.cont := Continuation(Continuation.EP_COREMEM, Continuation.EP_POINTERCHASE)

    access.addr := pc.addr
    access.length := pc.structSize.resized
  }
  val commitWait = commitWaitF.queue(64)

  // judge on inputs and try to
  // TODO: current limitation: <= 64B
  parser.io.headerOut.freeRun()
  val entryStream = commitWait >*< parser.io.dataOut.takeFirst.toStream.stage()

  val Seq(entryNext, entryResult) = StreamDemux2(entryStream, isFinish(entryStream.payload))

  // TODO: add return from local
  // need to change: Header cont & value
  pcBuilder.io.headerIn.translateFrom (entryResult) { (access, pair) =>
    val prev = pair.fst
    val hit = isHit(pair)
    access.allowOverride
    access.header := prev.header
    access.header.reqType := LegoMem.RequestType.READ
    access.header.reqStatus := Mux(hit, LegoMem.RequestStatus.OKAY, LegoMem.RequestStatus.ERR_FAIL)
    access.header.size := access.packedBytes
    // Pop omit value access here
    when (!hit) { access.header.cont := LegoMem.Continuation(LegoMem.Continuation.EP_NETWORK) }
    access.addr := seg(pair.snd, prev.valueOffset)
    access.length := prev.valueSize.resized
  }
  pcBuilder.io.dataIn.disable

  waitQueue.io.push.translateFrom (entryNext) { (pc, pair) =>
    val prev = pair.fst
    pc := prev
    pc.allowOverride
    // Update pointer addr
    pc.addr := seg(pair.snd, Cat(prev.nextOffset, U"3'b000").asUInt)
    // Consider flags
    when (pc.flags.useDepth) { pc.depth := prev.depth - 1 }
  }

  // Get segment from bits using offset
  def seg(bits : Bits, offset : UInt) : UInt = (bits |>> (offset << 3)).asUInt.resized

  // TODO: add key mask here
  def isFail(pair : Pair[PointerChasingHeader,Bits]): Bool = pair.fst.flags.useDepth && pair.fst.depth === 1
  def isHit(pair : Pair[PointerChasingHeader,Bits]): Bool = seg(pair.snd, pair.fst.keyOffset) === pair.fst.key
  def isFinish(pair : Pair[PointerChasingHeader,Bits]): Bool = isHit(pair) || isFail(pair)
}
