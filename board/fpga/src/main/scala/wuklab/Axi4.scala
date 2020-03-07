package wuklab

import spinal.lib._
import spinal.core._
import spinal.lib.bus.amba4.axi._

case class Axi4WriteOnlyNonBurstUpsizer(inputConfig : Axi4Config, outputConfig : Axi4Config) extends Component {
  val io = new Bundle {
    val input = slave(Axi4WriteOnly(inputConfig))
    val output = master(Axi4WriteOnly(outputConfig))
  }

  // This use a narrow burst
  val (cmd, data) = StreamFork2(io.input.writeCmd)
  cmd drive io.output.writeCmd
  io.output.writeRsp drive io.input.writeRsp

  val dataCtrl = new Area {
    val sink = io.output.writeData
    val stream = io.input.writeData

    // get the shift width
    val shiftWidth = data.addr & ((1 << outputConfig.bytePerWord) - 1)

    sink.arbitrationFrom(StreamJoin.arg(data, stream))
    sink.data := stream.data.resize(sink.data.getWidth) |<< shiftWidth
    Axi4Priv.driveWeak(stream,sink,stream.strb,sink.strb,() => B(sink.strb.range -> true),true,false)
    Axi4Priv.driveWeak(stream,sink,stream.user,sink.user,() => B(sink.user.range -> false),true,true)
    // Since this is a non-burst, this is always a last
    Axi4Priv.driveWeak(stream,sink,stream.last,sink.last,() => True,false,true)

  }

}