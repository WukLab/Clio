package wuklab

import spinal.core._
import spinal.lib._

import Utils._

// TODO: only take highest bit on match function
class LegoMemMultiDataInterface(operations : Seq[UInt => Bool])(implicit config : LegoMemConfig) extends Component {
  val numPorts = operations.size
  val io = new Bundle {
    val in = slave (LegoMemRawDataInterface())
    val out = Vec(master (LegoMemRawDataInterface()), numPorts)
  }

  // data dispatch
  val nextPort = RegNextWhenBypass(LegoMemHeader(io.in.dataIn.fragment).reqType |> getDemuxPort, io.in.dataIn.first)
  val inPorts = StreamDemux(io.in.dataIn, nextPort, numPorts)
  (io.out, inPorts).zipped map (_.dataIn <-< _)

  // data arbitrate
  // TODO: check the stage here
  io.in.dataOut << StreamArbiterFactory.lowerFirst.fragmentLock.on(io.out.map(_.dataOut.stage()))

  def getDemuxPort(opCode : UInt) = OHToUInt(Vec(operations.map(_(opCode))))
}

class LegoMemMultiControlEndpoint(addrs : Seq[UInt => Bool]) extends Component {
  val numPorts = addrs.size
  val io = new Bundle {
    val in = slave (LegoMemControlEndpoint())
    val out = Vec (master (LegoMemControlEndpoint()), numPorts)
  }

  // input path
  val validVec = addrs.map(_(io.in.in.addr))
  val valid = validVec.reduce(_ || _)
  val inPorts = StreamDemux(io.in.in.takeWhen(valid), OHToUInt(validVec), numPorts)
  (io.out, inPorts).zipped map (_.in <-< _)

  // output path
  io.in.out <-< StreamArbiterFactory.roundRobin.on(io.out.map(_.out))
}