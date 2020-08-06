package wuklab

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axilite._

// endpoint pipes: pipes
import Utils._

//class ReplicatorEndPointPipe(implicit config: LegoMemConfig) extends Component {
//  val replyQueueSize = 32
//  val sendQueueSize = 32
//
//  val numReplications = 2
//
//  val io = new Bundle {
//    val epIn = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
//    val epOut = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig).flip()
//
//    val regBus = slave (AxiLite4(AxiLite4Config(8, 32)))
//  }
//
//  val targetIp    = Vec(Reg (UInt(32 bits)) init 0, 2)
//  val targetSess  = Vec(Reg (UInt(32 bits)) init 0, 2)
//  val regs = new AxiLite4SlaveFactory(io.regBus)
//  regs.readAndWrite(targetIp(0), 0)
//  regs.readAndWrite(targetIp(1), 4)
//  regs.readAndWrite(targetSess(0), 8)
//  regs.readAndWrite(targetSess(1), 12)
//
//  // forward Ctrl
//  io.epIn.ctrlIn >> io.epOut.ctrlIn
//  io.epIn.ctrlOut << io.epOut.ctrlOut
//
//  // Data In
//  val receiveCtrl = new Area {
//    val Seq(replyMessage, sendMessageF, forwardMessage) =
//      io.epIn.dataIn.demux(generateInputSel(io.epIn.dataIn.tdata))
//
//    // Responses follows order, so this will work
//    val replies = (StreamFork(replyMessage.stage(), numReplications), targetIp).zipped
//      .map { (rep, ip) =>
//        rep.takeFirst
//          .takeBy(isReplicatedReply(ip))
//          .toEvent().queue(replyQueueSize)
//      }
//
//    // Send path
//    val msgLength = 64
//    val outputFire = Bool
//
//    val counter = Counter(2, outputFire)
//    val (stationMessageF, sendOutMessage) = StreamFork2(sendMessageF)
//    val stationMessage = stationMessageF.queue(msgLength)
//
//    val outputMessage = StreamMux(counter.value, Seq(sendOutMessage, stationMessage))
//        .changeFirst { payload =>
//          payload.tdata := LegoMemHeader.assignToBitsOperation { header =>
//            header.destIp   := targetIp  (counter.value)
//            header.destPort := targetSess(counter.value)
//          } (payload.tdata)
//        }
//    outputFire := outputMessage.fire
//  }
//
//  // Data Out
//  val sendPath = new Area {
//    val Seq(replySelfF, forwardOut) = io.epOut.dataOut.demux(isReplicatedReply(io.epOut.dataOut.payload).asUInt, 2)
//    val replySelf = replySelfF.takeFirst.queue(replyQueueSize)
//
//    val replyStream = receiveCtrl.replies.foldLeft(replySelf)(_ <* _)
//
//    io.epIn.dataOut << StreamArbiterFactory.lowerFirst.fragmentLock
//      .onArgs(receiveCtrl.outputMessage.stage(), replyStream.stage(), forwardOut.stage())
//  }
//
//  // 0 -> forward, 1 -> send, 2 -> reply
//  def generateInputSel(bits : Bits) : UInt = {
//    import LegoMem.RequestType._
//    val header = LegoMemHeader(bits)
//    val sel = UInt(2 bits)
//    when (header.reqType is WRITE_REP) {
//      sel := 1
//    } elsewhen (header.reqType is WRITE_REPR) {
//      sel := 2
//    } otherwise {
//      sel := 0
//    }
//    sel
//  }
//  def isReplicatedReply(source : UInt)(header : Fragment[AxiStreamPayload]) : Bool = True
//  def isReplicatedReply(header : Fragment[AxiStreamPayload]) : Bool = True
//}

class MigrationEndPointPipe(implicit config: CoreMemConfig) extends Component {
  val numRep = config.migrationCount

  val io = new Bundle {
    val epIn = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val epOut = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig).flip()
  }

  // forward Ctrl & dataOut
  io.epIn.ctrlIn >> io.epOut.ctrlIn
  io.epIn.ctrlOut << io.epOut.ctrlOut
  io.epIn.dataOut << io.epOut.dataOut

  // repeat when
  val isMig = LegoMemHeader(io.epIn.dataIn.tdata).reqType === LegoMem.RequestType.READ_MIG
  val counter = Counter(numRep, io.epOut.dataIn.lastFire && isMig)

  // If our packet is header only
  io.epOut.dataIn.translateFrom (io.epIn.dataIn) { (dataOut, dataIn) =>
    dataOut := dataIn
    val headerOut = LegoMemAccessHeader(config.virtualAddrWidth).fromWiderBits(dataIn.fragment.tdata)
    val headerIn = LegoMemAccessHeader(config.virtualAddrWidth).fromWiderBits(dataIn.fragment.tdata)
    when (isMig) {
      headerOut.addr := headerIn.addr + (counter.value.resize(config.virtualAddrWidth) |<< log2Up(config.migrationStep))
      dataOut.fragment.tdata := headerOut.asBits.resized
    }
  }
  io.epIn.dataIn.ready.removeAssignments()
  // The condition is
  // (If it is an migration && if if last)
  io.epIn.dataIn.ready := io.epOut.dataIn.ready && (!counter.willIncrement || counter.willOverflow)
}
