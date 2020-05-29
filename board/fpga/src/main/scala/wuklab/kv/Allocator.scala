package wuklab.kv

import spinal.core._
import spinal.lib._
import wuklab._

// Also need: pid & HTE Base addr
import Utils._

// TODO we NEED a tag here
case class AllocationRequest(reqWidth : Int) extends Bundle {
  val size = UInt(reqWidth bits)
  val cmd = UInt(4 bits)
}
case class AllocationInterface(reqWidth : Int, addrWidth : Int, numResponses : Int) extends Bundle with IMasterSlave {
  val request = Stream(AllocationRequest(reqWidth))
  val response = Vec (Stream (UInt(addrWidth bits)), numResponses)

  override def asMaster(): Unit = {
    master (request)
    response.foreach (slave (_))
  }

  def << (other : AllocationInterface) : Unit = {
    request << other.request
    (response, other.response).zipped map (_ >> _)
  }
  def >> (other : AllocationInterface) : Unit = other << this
}

// TODO: a high level abstraction: merger
class KeyValueVirtualAllocator(numPorts : Int)(implicit config : KeyValueConfig) extends Component {
  val fifoLength = 32
  val io = new Bundle {
    val ctrl = slave (LegoMemControlEndpoint())
    // Allocation interface?
    val allocs = slave (config.allocInterfaceType)
  }

  io.ctrl.out.translateFrom (io.allocs.request) { (cmd, size) =>
    cmd.epid := config.allocEp
    cmd.addr := config.allocAddr
    cmd.cmd := size.cmd
    cmd.beats := 0
    cmd.assignParam(size.size)
  }

  // 0 -> PTE alloc
  // 1 -> ENTRY ALLOC
  val returnPorts = StreamDemux(
    io.ctrl.in.fmap(_.resizeParam(config.addrWidth)),
    io.ctrl.in.addr(log2Up(numPorts)-1 downto 0),
    numPorts)
  (returnPorts, io.allocs.response).zipped map (_.queue(fifoLength) >> _)

  // arbiter
  // TODO: save this for later other usage!
//  val arbiter = StreamArbiterFactory.lowerFirst.fragmentLock.build(config.allocInterfaceType.request.payload, numPorts)
//
//  val (output, routeF) = StreamFork2(arbiter.io.output)
//  val route = routeF.translateWith(arbiter.io.chosen).queueLowLatency(config.allocLatency)
//
//  io.ctrl.out.translateFrom (output) { (cmd, size) =>
//    cmd.epid := config.allocEp
//    cmd.addr := config.allocAddr
//    cmd.cmd := config.allocCmd
//    cmd.assignParam(size)
//  }
//
//  val returnPorts = StreamDemux(route *> io.ctrl.in.fmap(_.resizeParam(config.addrWidth)), route.payload, numPorts)
//  (returnPorts, io.allocs).zipped map (_ >> _.response)
}
