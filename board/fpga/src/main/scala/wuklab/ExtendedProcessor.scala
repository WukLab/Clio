package wuklab

import spinal.core._
import spinal.lib._

import Utils._

// class ExtendedProcessor extends Component {
//
//   val selectionQueue = Stream(UInt(8 bits))
//
// }
//
// // Require data? or not
// trait ExtendedFunction[T <: Data] {
//   // This function have a timing constrain : finish in 1 cycle
//   def opcode : UInt
//   def processData(bitsIn : Bits, bitsOut : Bits) : Unit
// }
//
// class TestAndSet extends ExtendedFunction[NoData] {
//   override def opcode = LegoMem.RequestType.READ_DEREF_READ
//
//   override def processData(headerIn: Stream[LegoMemHeader], headerOut: Stream[LegoMemHeader], dataIn: Stream[Bits], dataOut: Stream[Bits]): Unit = {
//
//   }
// }

class PingPong(dataLength : Int)(implicit config : CoreMemConfig) extends Component with XilinxAXI4Toplevel {
  require(dataLength <= (config.epDataAxisConfig.dataBytes - LegoMemHeader.headerBytes))

  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
  }

  val bridge = new RawInterfaceEndpoint
  bridge.io.ep <> io.ep

  bridge.io.raw.disableCtrl
  bridge.io.raw.dataOut.translateFrom (bridge.io.raw.dataIn.takeFirst) { (newData, oldData) =>
    val assignF = LegoMemHeader.assignToBitsOperation { header =>
      header.cont := U"16'h0"
      header.size := LegoMemHeader.headerBytes + dataLength
    }

    newData.fragment := assignF(oldData.fragment)
    newData.last := True
  }

  addPrePopTask(renameIO)
}

