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
    access.header.reqType := Mux(hit, LegoMem.RequestType.READ, LegoMem.RequestType.READ_RESP)
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
  def isFail(pair : Pair[PointerChasingHeader,Bits]): Bool =
    pair.fst.flags.useDepth && pair.fst.depth === 1
  def isHit(pair : Pair[PointerChasingHeader,Bits]): Bool =
    seg(pair.snd, pair.fst.keyOffset) === pair.fst.key
  def isNull(pair : Pair[PointerChasingHeader,Bits]): Bool =
    seg(pair.snd, Cat(pair.fst.nextOffset, U"3'b000").asUInt) === U"64'h0"

  def isFinish(pair : Pair[PointerChasingHeader,Bits]): Bool =
    isHit(pair) || isFail(pair) || isNull(pair)
}

// Data frame

//class DataMapCore(implicit config : CoreMemConfig) extends Component {
//  val numOpCodes = 4;
//
//  val io = new Bundle {
//    val dataIn = slave Stream Fragment (Bits(512 bits))
//    val parameterX = in UInt(64 bits)
//    val parameterY = in UInt(64 bits)
//    val opCode = in Bits(8 bits)
//
//    val dataOut = master Stream Fragment (Bits(512 bits))
//  }
//
//  // We do not use data MaskIn, just process all!
//  val streamsIn = StreamFork(io.dataIn, numOpCodes)
//
//  def mapPlus(v : UInt) : UInt = v + io.parameterX
//  def mapMinus(v : UInt) : UInt = v - io.parameterY
//  def filterEQ(v : UInt) : UInt = Mux(v === io.parameterX, v, 0)
//  def filterNE(v : UInt) : UInt = Mux(v =/= io.parameterX, v, 0)
//
//  val opFuncs = Seq[UInt => UInt](mapPlus, mapMinus, filterEQ, filterNE)
//
//  val streamsOut = (streamsIn, opFuncs).zipped.map { (data, op) =>
//    data.liftStream(_
//        .as(Vec(UInt(64 bits), 8))
//        .map(op)
//        .map(_.asBits)
//        .reduce(_ ## _)
//    )
//  }
//
//}

//class DataProcessKernel[T <: Data](dataType : T, functions : Seq[(DataFrameKernelParameter, Bits) => T])
//  (implicit config : CoreMemConfig) extends Component {
//
//  val numOpCodes = functions.size
//
//  val io = new Bundle {
//    val dataIn = slave Stream Fragment (Bits(512 bits))
//    val param = slave Stream DataFrameKernelParameter()
//
//    val dataOut = master Stream Fragment (dataType)
//  }
//
//  val maxPipelineLength = 8
//
//  // TODO: add parameters
//  val streamsIn = io.dataIn.demux(io.param.fmap(_.dfOp), numOpCodes)
//  val streamsOut = (streamsIn, functions).zipped.map { (data, func) =>
//    data.liftStream(func(io.param.payload, _))
//  }
//  io.dataOut << StreamFragmentArbiter(dataType)(streamsOut)
//
//}

class DataFrameEndpoint(implicit config : CoreMemConfig) extends Component with XilinxAXI4Toplevel {
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

  val engine = new DataProcessEngine
  engine.io.reqEp <> multiData.io.out(0)
  engine.io.vmEp <> multiData.io.out(1)

  addPrePopTask(renameIO)
}

class DataProcessEngine(implicit config : CoreMemConfig) extends Component {
  val io = new Bundle {
    val reqEp = slave (LegoMemRawDataInterface())
    val vmEp = slave (LegoMemRawDataInterface())
  }

  // Pointer chasing end point
  val reqHeaderIn = io.reqEp.dataIn
    .takeFirst.toStream
    .fmap(PointerChasingHeader().fromWiderBits)
  val (reqHeader, waitHeader) = StreamFork2(reqHeaderIn)

  val repBuilder = new PacketBuilder(512, LegoMemAccessHeader(config.virtualAddrWidth))
  repBuilder.io.dataOut >> io.reqEp.dataOut

  // Virtual Memory Endpoint
  val parser = new PacketParser(512, LegoMemAccessHeader(config.virtualAddrWidth))
  parser.io.dataIn << io.vmEp.dataIn
  parser.io.headerOut.freeRun()

  val builder = new PacketBuilder(512, LegoMemAccessHeader(config.virtualAddrWidth))
  builder.io.dataOut >> io.vmEp.dataOut

  // Step 1: submit to VMEP. cache the header
  builder.io.dataIn.disable
  builder.io.headerIn.translateFrom (reqHeader) { (access, pc) =>
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

  // Step 2: do computation
  val (computeHeader, repHeader) = StreamFork2(waitHeader.queue(64))
  val compute = new Area {

    val datas = parser.io.dataOut.demux(computeHeader.fmap(h => h.key.asBits(0).asUInt), 2)

    // path 0, simple filter
    val lengthQueue = StreamFifo(UInt(16 bits), 8)
    val dataOut0F = datas(0).throwBy(f => f.fragment(31 downto 0).asUInt === 0)
    val dataOut0 = dataOut0F.queue(64)
    val counter = Reg (UInt(16 bits)) init 0
    val counterNext = Mux(dataOut0F.first, U"16'd64", counter + U"16'd64")
    when (dataOut0F.fire) { counter := counterNext }

    lengthQueue.io.push.payload := counterNext
    lengthQueue.io.push.valid := dataOut0F.lastFire

    // path 1, simple sum
    datas(1).freeRun()
    val dataOut1 = StreamFifo(Fragment(Bits(512 bits)), 8)
    val regs = Array.fill(8) { Reg (UInt(64 bits)) init 0 }
    val vec = datas(1).fragment.as(Vec(UInt(64 bits), 8))
    val sumVec = (regs, vec).zipped.map(_ + _)
    (regs, vec, sumVec).zipped.foreach { (reg, e, s) =>
      when (datas(1).fire) {
        when (datas(1).first) {
          reg := e
        } otherwise {
          reg := s
        }
      }
    }

    dataOut1.io.push.valid := datas(1).lastFire
    dataOut1.io.push.fragment := sumVec.map(_.asBits).reduce(_ ## _)
    dataOut1.io.push.last := True

    // merge
    val dataOut = StreamFragmentArbiter(Bits(512 bits))(Seq(dataOut0, dataOut1.io.pop))
  }

  // Step 3: generate reply
  // where to send back? do the header change!
  val repHeaderF = repHeader.queue(8)
  compute.lengthQueue.io.pop.ready := repHeaderF.fire && !repHeaderF.key(0)
  repBuilder.io.headerIn.translateFrom (
    repHeaderF.haltWhen(!repHeaderF.key(0) && !compute.lengthQueue.io.pop.valid)
  ) { (rep, pc) =>
    rep.allowOverride
    rep.header := pc.header
    rep.header.reqType := LegoMem.RequestType.READ_RESP
    rep.header.reqStatus := LegoMem.RequestStatus.OKAY

    val size = Mux(pc.key(0), U"16'd64", compute.lengthQueue.io.pop.payload)

    rep.header.size := size + rep.packedBytes
    import LegoMem.Continuation
    rep.header.cont := Continuation(Continuation.EP_NETWORK)

    rep.addr := pc.addr
    rep.length := size.resized
  }
  // Build data...
  repBuilder.io.dataIn << compute.dataOut


//  def filterOperation(
//    filter : (UInt,  => Bool,
//    dataStream : Stream[Fragment[Bits]],
//
//  ) {
//    def sort(l : (UInt, Bool), r : (UInt, Bool)) : ((UInt, Bool), (UInt, Bool)) = {
//      val rl = (UInt(2 bits), Bool)
//      val rr = (UInt(2 bits), Bool)
//      rl._1 := l._1
//      rl._2 := l._2
//      rr._1 := r._1
//      rr._2 := r._2
//      when (r._2 && !l._2) {
//        rl._1 := r._1
//        rl._2 := r._2
//        rr._1 := l._1
//        rr._2 := l._2
//      }
//      (rl, rr)
//    }
//    // step 1: compare, vec -> (vec, bool)
//    val dataReg  = Vec(vec.map(u => RegNext(u)), UInt(64 bits))
//    val condReg = Vec(vec.map(u => filter(u)),  Bool)
//    // step 2: sort net, 1 stage
//    val (l20, l21) = sort((U"2'h0", condReg(0)), (U"2'h1", condReg(1)))
//    val (l22, l23) = sort((U"2'h2", condReg(2)), (U"2'h3", condReg(3)))
//
//    val (l31, l33) = sort(l21, l23)
//    val (l30, l32) = sort(l20, l22)
//
//    val (l41, l42) = sort(l31, l32)
//
//    val sortedData = Seq(dataReg(l30._1), dataReg(l41._1), dataReg(l42._1), dataReg(l33._1))
//    val sortedBool = Seq(l30._2, l41._2, l42._2, l33._2)
//
//    // step 3: shift * push into fifos
//    val curIndex = Reg (UInt(2 bits)) init 0
//    val fifos = Seq.fill(4) { StreamFifo(UInt(64 bits), 4) }
//    val fifoPush = Vec(fifos.map(_.io.push), Stream(UInt(64 bits)))
//
//    // map different to different port
//    sortedData.zipWithIndex.map { case (data, idx) =>
//      val fifoIdx = U(idx, 2 bits) + curIndex
//      fifoPush(fifoIdx).valid := sortedBool(idx)
//      fifoPush(fifoIdx).payload := data
//    }
//    // update lastIndex
//    // TODO: this num Valid?
//    val numValid = UInt(2 bits)
//    curIndex := curIndex + numValid
//
//    // step 4: pop
//    val allValid = fifos.map(_.io.pop.valid).reduce(_ && _)
//    val allData = fifos.map(_.io.pop.payload.asBits).reduce(_ ## _)
//
//    // final fifo
//    val outputFIFO = StreamFifo(Bits(512 bits), 8)
//    outputFIFO.io.push.payload := allData.asBits
//    outputFIFO.io.push.valid := allValid
//
//  }
//
//  def mapOperation(f : (UInt, UInt, UInt) => UInt)
//  def reduceOperation(f : (UInt, UInt, UInt) => UInt, r : (UInt, UInt) => UInt)

}
