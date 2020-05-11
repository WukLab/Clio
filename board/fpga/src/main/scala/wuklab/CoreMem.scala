package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Aw, Axi4Config, Axi4ReadOnly, Axi4W, Axi4WriteOnly, Axi4WriteOnlyUpsizer}
import spinal.lib.bus.amba4.axilite.AxiLite4
import spinal.lib.fsm.{EntryPoint, State, StateMachine}


// Input : Pte Without physical addr (ppa)
// Output : Pte With ppa
// (ppaWidth : Int, tagWidth : Int, numPageSizes : Int)

class PageFaultUint(implicit config : CoreMemConfig) extends Component {
  // TODO: init unused registers
  val reportWidth = 8

  val io = new Bundle {
    val req       = slave  Stream config.pteWithAddrType
    val res       = master Stream config.pteResType
    val rpt       = master Flow config.pteWithAddrType
    val addrFifoRpt = master Stream UInt(reportWidth bits)
    val addrFifos = Vec(slave Stream UInt(config.physicalAddrWidth bits), config.numPageSizes)

    val memWriteRes = slave Flow UInt(config.pageFaultCacheCellWidth bits)
    val memWriteData = master Stream config.pteWithAddrType
    val memWriteUser = master Stream UInt(config.pageFaultCacheCellWidth bits)
  }

  val cam = new LookupTableStoppable(config.tagWidth, config.pteCompactWidth, config.numPageFaultCacheCells)

  // The page fault Path
  val pageFaultPath = new Area {
    cam.io.wr.shootDown << io.memWriteRes

    cam.io.rd.req.translateFrom (io.req) ((cmd, pte) => cmd.key := pte.tag)

    // delayed pipeline interface
    val originalPte = cam.delay(io.req.payload)
    val lookupResult = cam.io.rd.res.fmap(r => {
      val sel = r.hit && originalPte.used
      Mux(sel, PageTableEntry.compact(originalPte, r.value.asBits), originalPte)
    })

    val selectedAddr = StreamMux(lookupResult.pageType, io.addrFifos.toSeq)
    // merge when
    val afterAlloc = Stream(Pair(Bool, lookupResult.payloadType()))
    val mergeCtrl = new Area {
      val noAlloc = lookupResult.used || !lookupResult.allocated
      lookupResult.ready := Mux(noAlloc, afterAlloc.ready, afterAlloc.ready && selectedAddr.valid)
      selectedAddr.ready := !noAlloc && afterAlloc.ready && lookupResult.valid
      afterAlloc.valid := Mux(noAlloc, lookupResult.valid, lookupResult.valid && selectedAddr.valid)
      afterAlloc.payload.fst := noAlloc
      afterAlloc.payload.snd := lookupResult.payload
      when (!noAlloc) {
        afterAlloc.snd.used := True
        afterAlloc.snd.ppa := selectedAddr.payload
      }
    }
  }

  // Report to the control agent
  // Send to the write unit
  val Seq(bramWr, faultRpt, faultFwd) = StreamFork(pageFaultPath.afterAlloc.stage, 3)

  // TODO: check this write path, add new content
  cam.io.wr.req.translateFrom (bramWr.throwWhen(bramWr.fst)) ((next, pte) => {
    next.value := pte.snd.asCompactBits.asUInt
    next.key := pte.snd.tag
    next.mask := 0
  })

  val pageFaults = faultRpt.throwWhen(faultRpt.fst).fmap(_.snd)
  io.memWriteData << pageFaults
  io.memWriteUser << cam.io.wr.res.asStream

  io.res << faultFwd.fmap(_.snd)
  // Only report valid ones
  // TODO: add more resource here
  io.rpt << faultFwd.tapAsFlow.fmap(_.snd).takeBy(_.allocated)

  val addrFireVec = Vec(io.addrFifos.map(_.fire))
  io.addrFifoRpt << ReturnStream(addrFireVec.asBits.asUInt.resize(8), addrFireVec.orR)
}

// (ppaWidth : Int, tagWidth : Int, busAddrWidth : Int, userWidth : Int)
class PageTableWriter(implicit config: CoreMemConfig) extends Component {
  val memoryLatency = 32

  val io = new Bundle {
    val bus = master (Axi4WriteOnly(config.pageTableWriterAxi4Config))
    val reqData = slave Stream config.pteWithAddrType
    val reqUser = slave Stream UInt(config.pageFaultCacheCellWidth bits)
    val res = master Flow UInt(config.pageFaultCacheCellWidth bits)
  }

  val reqUser = io.reqUser.queueLowLatency(1)
  val reqData = io.reqData.queueLowLatency(1)

  val userFifo = StreamFifo(UInt(config.pageFaultCacheCellWidth bits), memoryLatency)

  reqUser >> userFifo.io.push

  val (cmd, data) = StreamFork2(reqData)
  io.bus.writeCmd.translateFrom  (cmd)  ((cmd, pte) => {
    cmd.addr := config.getBucketAddr(pte.pteAddr)
    cmd.size := Axi4.size.BYTE_16.asUInt
  })
  io.bus.writeData.translateFrom (data) ((cmd, pte) => cmd.data := pte.asFullBits)

  io.res << (io.bus.writeRsp.stage() *> userFifo.io.pop).toFlow
}

trait HashFunction extends StoppablePipeline {
  val outputWidth : Int
  def hash(input : UInt) : UInt
}

// offset: related to page number
class LinearHashFunction(val offset: Int, val outputWidth: Int, enable : Bool) extends HashFunction {
  override def pipelineDelay = 1

  override def hash(input: UInt) : UInt = {
    RegNextWhen(input >> offset, enable).resize(outputWidth bits)
  }

  override def enableSignal = enable

}

class Lookup3HashFunction(val offset: Int, val outputWidth : Int, enable : Bool) extends HashFunction {
  override def hash(input: UInt) : UInt = {
    val inputWidth = input.getWidth
    val comp = new Lookup3(0)(inputWidth, outputWidth)
    comp.io.enable := enable
    comp.io.data := input
    comp.io.hash
  }
  override def enableSignal = enable
  override def pipelineDelay = 3
}

class FetchUnit(pageType : Int)(implicit config: CoreMemConfig) extends Component {

  // Select hash function here
  type Hash = LinearHashFunction

  val io = new Bundle {
    val bus = master (Axi4ReadOnly(config.lookupAxi4Config))
    val req = slave Stream config.lookupReqType
    val res = master Stream config.pteWithAddrType
  }

  val fifo = new StreamFifoLowLatency(io.bus.readCmd.payloadType, 1, 0)
  val (readCmd, readRsp) = WaterMarkFifo(fifo.io.pop, io.bus.readRsp.fmap(_.data), 32)

  // These fifos are part of the watermark system
  val reqFifo = StreamFifo(new Bundle {
    val tag = cloneOf(io.req.tag)
    val seqId = cloneOf(io.req.seqId)
    val addr = UInt(config.bucketNumberWidth bits)
  }, 64)

  val hashCtrl = new Area {
    val halt = fifo.io.push.ready
    io.req.ready := halt

    // TODO: check this, the valid signal is not using.
    val hashFunc = new Hash(config.pageOffsets(pageType), config.bucketNumberWidth, halt)
    val readAddr = new Flow(UInt(config.bucketNumberWidth bits))
    readAddr.payload := hashFunc.hash(config.tag2fullTag(io.req.tag))
    readAddr.valid := hashFunc.delay(io.req.fire) init False

    val delayedReq = hashFunc.delayFlow(io.req.payload, io.req.fire)
    reqFifo.io.push.tag := delayedReq.tag
    reqFifo.io.push.addr := readAddr.payload
    reqFifo.io.push.seqId := delayedReq.seqId
    reqFifo.io.push.valid := delayedReq.valid
  }

  val readCtrl = new Area {
    // use a fifo to set ready to high
    readCmd >> io.bus.readCmd

    val readValid = hashCtrl.readAddr.valid & fifo.io.push.ready

    fifo.io.push.valid := readValid
    fifo.io.push.addr := config.getBucketAddr(hashCtrl.readAddr.payload)
    fifo.io.push.size := Axi4.size.BYTE_64.asUInt
    fifo.io.push.len := config.linePerBucket - 1
    fifo.io.push.setBurstINCR
  }

  val matchCtrl = new Area {
    // TODO : find back pressure signal
    val ready = True

    val valid = RegNext(readRsp.fire) init False
    val hit = Reg(Bool) init False
    val selected = Reg (config.pteWithAddrType)

    readRsp.ready := ready

    val ptes = readRsp.payload
      .as(Vec(Bits(config.pteFullWidth bits), config.ptePerLine))
      .map(cloneOf(config.pteWithAddrType).fromFullBits(_))

    for ((pte, i) <- ptes.zipWithIndex) {
      pte.seqId := reqFifo.io.pop.seqId
      pte.pteAddr := reqFifo.io.pop.addr + (U(i, config.bucketNumberWidth bits) |<< log2Up(config.pteFullBytes))
    }
    val matchVec = ptes.map(pte => pte.allocated && tagMatch(pte, reqFifo.io.pop.tag))

    when (readRsp.fire) {
      selected := PriorityMux(matchVec, ptes)
      hit := matchVec.reduce(_ || _)
    }
  }

  val writeCtrl = new Area {
    val counter = Counter(0 until config.linePerBucket, matchCtrl.valid)

    // This all clear works for special bits
    // Designer should always use 0 for not enable, dont they?
    // TODO: optimize the bits, do not clear tag/pte
    val emptyPte = cloneOf(matchCtrl.selected)
    emptyPte.clearAll

    val pteReg = Reg(cloneOf(matchCtrl.selected)) init emptyPte
    val nextPte = Mux(matchCtrl.hit && matchCtrl.valid, matchCtrl.selected, pteReg)
    pteReg := Mux(counter.willOverflow, emptyPte, nextPte)

    io.res.valid := counter.willOverflow
    io.res.payload := nextPte
    io.res.payload.seqId.allowOverride
    io.res.payload.seqId := reqFifo.io.pop.seqId

    reqFifo.io.pop.ready := counter.willOverflow
  }

  def tagMatch(pte : PageTableEntry, input : UInt) : Bool = {
    require(widthOf(input) == widthOf(pte.tag))

    val res = pte.tag ^ input
    ~(config.getTagMask(pte.pageType) & res).orR
  }
}

object UpdateFunctions {
  // Update the PTE based on the hit infomation at cell level
  def fifoUpdate(alloc : Flow[UInt], hit : Flow[UInt], shootDown : Flow[UInt]) : Stream[UInt] = {
     alloc.asStream.queue(32)
  }
}

class UpdateUnit(
                  updateFunction : (Flow[UInt], Flow[UInt], Flow[UInt]) => Stream[UInt]
                )(implicit config : CoreMemConfig) extends Component {

  val io = new Bundle {
    // Communication FIFOs
    val rpt = new Bundle {
      val hit = slave Flow UInt(config.cacheCellWidth bits)
      val shootDown = slave Flow UInt(config.cacheCellWidth bits)
      val alloc = slave Flow UInt(config.cacheCellWidth bits)
    }

    val updateReq = slave Flow config.pteWithPpaType
    val insertReq = master Stream LookupWrite(config.tagWidth, config.pteCompactWidth, useUsed = true, usedWidth = config.cacheCellWidth)
  }

  // This flow is always ready
  val nextEvictionId = new Stream(UInt(config.cacheCellWidth bits))
  nextEvictionId << updateFunction(io.rpt.alloc, io.rpt.hit, io.rpt.shootDown)

  // Do not use destructive assignment
  // TODO: There will be replicated assignment, rethink this structure
  io.insertReq.translateFrom (io.updateReq.asStream) ((cmd, req) => {
    cmd.usedCell := nextEvictionId.payload
    cmd.key := req.tag
    cmd.mask := config.getTagMask(req.pageType)
    cmd.value := req.asCompactBits.asUInt
  })
  nextEvictionId.ready := io.insertReq.fire
}

// Wrapper of the cache, change io to the request, add mux and demux
class BramCache(implicit config : CoreMemConfig) extends Component {

  val table = new LookupTableStoppable(config.tagWidth, config.pteCompactWidth, config.numCacheCells,
    useUsed = true, autoUpdate = true, useHitReport = true)

  val io = new Bundle {
    val cmd = new Bundle {
      val req = slave Stream config.lookupReqType
      val fwd = master Stream config.lookupReqType
      val res = master Stream config.pteWithPpaType
    }

    val ctrl = new Bundle {
      val insertReq = slave Stream table.io.wr.req.payloadType()
      val hitRpt = master Flow UInt(config.cacheCellWidth bits)
      val allocRpt = master Flow UInt(config.cacheCellWidth bits)
      val shootDownRpt = master Flow UInt(config.cacheCellWidth bits)
    }

  }

  io.ctrl.insertReq >> table.io.wr.req
  io.ctrl.allocRpt << table.io.wr.res

  val lookupCtrl = new Area {
    io.cmd.req.fmap(r => LookupReadReq(r.tag)) >> table.io.rd.req
    // read out
    // TODO: fix this constant for shoot down
    val delayedCmd = table <|| io.cmd.req
    val isShootDown = delayedCmd.reqType === AddressLookupRequest.RequestType.SHOOTDOWN
    val isResult = table.io.rd.res.hit

    val Seq(fwd, res) = isResult.demux(table.io.rd.res.throwWhen(isShootDown))
    // forward path
    io.cmd.fwd <-< (delayedCmd <* fwd)
    // result path
    val pte = io.cmd.res.payloadType()
    pte.seqId := delayedCmd.seqId
    io.cmd.res << res.fmap(r => pte.fromCompactBits(r.value.asBits))

    val Seq(hitRpt, shootDownRpt) = isShootDown.demux(table.io.rd.rpt)
    io.ctrl.hitRpt << hitRpt
    io.ctrl.shootDownRpt << shootDownRpt
    table.io.wr.shootDown << shootDownRpt
  }
}

// TODO: make this a factory
// Basically control agent is a fifo arbiter. It handles two kinds of FIFOs: Sync ones and Async ones.
// SoC should make sure the FIFO is not blocked. It also handles the number of the names
class LookupControlAgent(implicit config : CoreMemConfig) extends Component {
  val asyncFifoWidth = config.physicalAddrWidth
  val io = new Bundle {
    val cmdIn = slave Stream ControlRequest()
    val cmdOut = master Stream ControlRequest()

    // master control signals
    val asyncFifos = Vec(master Stream UInt(asyncFifoWidth bits), config.numPageSizes)

    // report to master
    val hitRpt = slave Flow UInt(config.cacheCellWidth bits)
    val addrFifoRpt = slave Stream UInt(8 bits)
  }

  // TODO: add fifo length here

  // Write Command
  val cmdIn = io.cmdIn.queue(16)
  val numOutputFifos = config.numPageSizes
  val inputs = StreamDemux(cmdIn, cmdIn.addr(log2Up(numOutputFifos)-1 downto 0), numOutputFifos)

  val (asyncFifos, freeCounts) = inputs
    .map { _.fmap(_.resizeParam(asyncFifoWidth)).queueWithAvailability(32) }
    .unzip
  (asyncFifos, io.asyncFifos).zipped map (_ >> _)

  // Read Command
  io.cmdOut << StreamArbiterFactory.onArgs(
    io.addrFifoRpt.fmap(u => ControlRequest(LegoMem.Continuation.EP_SOC, param8 = u))
  )

  //  io.cmdOut << StreamArbiterFactory.onArgs(
  //    io.hitRpt.asStream.fmap(u => ControlRequest(0x10, param32 = u.resize(32))),
  //    io.pageFaultRpt.asStream.fmap(u => ControlRequest(0x11, param32 = u.resize(32)))
  //  )


}



class AddressLookupUnit(implicit config : CoreMemConfig) extends Component {
  // TODO: make this one large enough
  val numInflightRequests = 64

  // input: request
  val io = new Bundle {
    val req = slave Stream config.lookupReqType
    val res = master Stream config.pteResType
    val ctrl = new Bundle {
      val in = slave Stream ControlRequest()
      val out = master Stream ControlRequest()
    }

    val bus = master (Axi4(config.lookupAxi4Config))
  }

  // TODO: rething this arrows. if theres timing violation, first add pipelines here
  // Control Agent
  val agent = new LookupControlAgent
  io.ctrl.in >> agent.io.cmdIn
  io.ctrl.out << agent.io.cmdOut

  val seqFifo = ReturnStream(io.req.seqId, io.req.fire).queue(numInflightRequests)

  // bram cache & update unit
  val cache = new BramCache
  val update = new UpdateUnit(UpdateFunctions.fifoUpdate)
  cache.io.cmd.req << io.req
  cache.io.ctrl.insertReq << update.io.insertReq
  update.io.rpt.hit << cache.io.ctrl.hitRpt
  update.io.rpt.alloc << cache.io.ctrl.allocRpt
  update.io.rpt.shootDown << cache.io.ctrl.shootDownRpt

  cache.io.ctrl.hitRpt >> agent.io.hitRpt

  // fetch unit
  // TODO: add cache at fetch
  val fetcher = new FetchUnit(0)
  fetcher.io.req << cache.io.cmd.fwd

  // pagefault unit
  val pageFault = new PageFaultUint
  val writer = new PageTableWriter
  pageFault.io.req << fetcher.io.res
  pageFault.io.memWriteRes << writer.io.res
  (pageFault.io.addrFifos, agent.io.asyncFifos).zipped map (_ << _)
  writer.io.reqData << pageFault.io.memWriteData
  writer.io.reqUser << pageFault.io.memWriteUser

  pageFault.io.rpt >> update.io.updateReq
  pageFault.io.addrFifoRpt >> agent.io.addrFifoRpt

  // bus assignment
  val writeAdapter = Axi4WriteOnlyNonBurstUpsizer(writer.io.bus.config, io.bus.config)
  writer.io.bus >> writeAdapter.io.input
  writeAdapter.io.output >> io.bus
  fetcher.io.bus >> io.bus

  // merge here
  // TODO: replace this with a reorder buffer (log the seq numbers, and compare)
  val streams = Seq(cache.io.cmd.res, pageFault.io.res)
  val validVec = streams.map(_.seqId === seqFifo.payload)
  val selected = StreamMux(OHToUInt(validVec), streams).continueWhen(validVec.reduce(_ || _))
  io.res << (seqFifo *> selected)

}

