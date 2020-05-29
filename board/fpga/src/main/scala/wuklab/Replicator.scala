package wuklab

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axilite._

// endpoint pipes: pipes
import Utils._

class ReplicatorEndPointPipe(implicit config: LegoMemConfig) extends Component {
  val replyQueueSize = 32
  val sendQueueSize = 32

  val io = new Bundle {
    val epIn = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val epOut = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig).flip()

    val regBus = slave (AxiLite4(AxiLite4Config(8, 32)))
  }

  val firstIp     = Reg (UInt(32 bits)) init 0
  val firstSess   = Reg (UInt(32 bits)) init 0
  val secondIp    = Reg (UInt(32 bits)) init 0
  val secondSess  = Reg (UInt(32 bits)) init 0
  val regs = new AxiLite4SlaveFactory(io.regBus)
  regs.readAndWrite(firstIp, 0)
  regs.readAndWrite(firstSess, 4)
  regs.readAndWrite(secondIp, 8)
  regs.readAndWrite(secondSess, 12)

  // forward Ctrl
  io.epIn.ctrlIn >> io.epOut.ctrlIn
  io.epIn.ctrlOut << io.epOut.ctrlOut

  // Data In
  val receiveCtrl = new Area {
    val Seq(replyMessage, sendMessageF, forwardMessage) = StreamFork(io.epIn.dataIn, 3)
    // TODO: filter the keep Message
    // Reply PAth
    val Seq(replyFirstF, replySecondF) = StreamFork(replyMessage.stage(), 2)
    val replyFirst  = replyFirstF.takeFirst.takeBy(isReplicatedReply).queue(replyQueueSize)
    val replySecond = replySecondF.takeFirst.takeBy(isReplicatedReply).queue(replyQueueSize)

    // Send path

  }

  // Data Out
  val sendPath = new Area {
    val (replySelfF, forwardOutF) = StreamFork2(io.epOut.dataOut)
    val replySelf = replySelfF.takeFirst.takeBy(isReplicatedReply).queue(replyQueueSize)
    val replyStream = replySelf <* receiveCtrl.replyFirst <* receiveCtrl.replySecond

    val forwardOut = forwardOutF.takeBy(!isReplicatedReply(_))

    io.epIn.dataOut << StreamArbiterFactory.lowerFirst.fragmentLock
      .onArgs(replyStream.stage(), forwardOut.stage())
  }

  def isReplicatedWrite(header : Fragment[AxiStreamPayload]) : Bool = True
  def isReplicatedReply(header : Fragment[AxiStreamPayload]) : Bool = True
  def sendReplicatedWrite(ip : UInt, sess : UInt)(header : AxiStreamPayload) : Unit = {

  }
}
