package wuklab.kv

import wuklab._
import spinal.core._
import spinal.lib._
import Utils._
import spinal.core.internals.Operator
import spinal.lib
import spinal.lib.bus.amba4.axi.Axi4
import spinal.lib.bus.amba4.axilite._

class KeyValuePhysicalEndpoint(implicit config : KeyValueConfig) extends Component with XilinxAXI4Toplevel {
  // Convert the DMA interface to the LegoMem
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    val access = master (Axi4(config.accessAxi4Config))
  }

  val bridge = new RawInterfaceEndpoint
  bridge.io.ep <> io.ep

  // Ctrl path
  def isContoller(addr : UInt) = addr === U"8'hFF"
  val multiCtrl = new LegoMemMultiControlEndpoint(Seq(isContoller, !isContoller(_)))
  multiCtrl.io.in.in << bridge.io.raw.ctrlIn
  multiCtrl.io.in.out >> bridge.io.raw.ctrlOut

  // Controller will provide register access interface for legomem
  val controller = new KeyValueController
  controller.io.ctrl <> multiCtrl.io.out(0)

  val allocator = new KeyValueVirtualAllocator(2)
  allocator.io.ctrl <> multiCtrl.io.out(1)

  // Engine ->
  val kv = new KeyValueEngine
  kv.io.ep.dataIn << bridge.io.raw.dataIn
  kv.io.ep.dataOut >> bridge.io.raw.dataOut
  kv.io.allocs <> allocator.io.allocs

  // dma -> LegoMemVirtualDMA
  val dma = new axi_dma(config.dmaConfig.streamConfig, config.accessAxi4Config, config.dmaAddrWidth, config.sizeWidth)
  dma.io.m_axi <> io.access
  val (writeCmd, sizeCmdF) = StreamFork2(kv.io.dma.write.cmd)
  val sizeCmd = sizeCmdF.fmap { cmd =>
    val len = cmd.len.resize(6)
    val lenTop = !len.orR
    val lenFull = (lenTop ## len).asUInt
    ((U"1'h1" << lenFull) - 1).resize(64)
  } .queueLowLatency(4)
  dma.io.m_axis_read_data.liftStream(_.tdata) >> kv.io.dma.read.data
  dma.io.s_axis_write_data.translateFrom (kv.io.dma.write.data.continueWhen(sizeCmd.valid)) { (axis, frag) =>
    axis.fragment.tdata := frag.fragment
    axis.fragment.tkeep := Mux(frag.last, sizeCmd.payload.asBits, B"64'hFFFF_FFFF_FFFF_FFFF")
    axis.last := frag.last
  }
  sizeCmd.ready := kv.io.dma.write.data.lastFire
  dma.io.s_axis_read_desc.translateFrom (kv.io.dma.read.cmd) { (axis, cmd) =>
    axis.len := cmd.len
    axis.addr := controller.io.regs.baseAddr + cmd.addr
  }
  dma.io.s_axis_write_desc.translateFrom (writeCmd.queueLowLatency(4)) { (axis, cmd) =>
    axis.len := cmd.len
    axis.addr := controller.io.regs.baseAddr + cmd.addr
  }

  dma.io.write_enable := True
  dma.io.read_enable := True
  dma.io.write_abort := False

  addPrePopTask(renameIO)
}

class KeyValueVirtualEndpoint(implicit config : KeyValueConfig) extends Component with XilinxAXI4Toplevel {
  // Convert the DMA interface to the LegoMem
  val io = new Bundle {
    val ep = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
  }

  val bridge = new RawInterfaceEndpoint
  bridge.io.ep <> io.ep

  // Ctrl path
  def isContoller(addr : UInt) = addr === U"8'hFF"
  val multiCtrl = new LegoMemMultiControlEndpoint(Seq(isContoller, !isContoller(_)))
  multiCtrl.io.in.in << bridge.io.raw.ctrlIn
  multiCtrl.io.in.out >> bridge.io.raw.ctrlOut

  // Controller will provide register access interface for legomem
  val controller = new KeyValueController
  controller.io.ctrl <> multiCtrl.io.out(0)

  val allocator = new KeyValueVirtualAllocator(2)
  allocator.io.ctrl <> multiCtrl.io.out(1)

  def isKeyValueOp(opcode : UInt) = LegoMem.RequestType.isKV(opcode)
  val multiData = new LegoMemMultiDataInterface(Seq(isKeyValueOp, !isKeyValueOp(_)))
  multiData.io.in.dataIn << bridge.io.raw.dataIn
  multiData.io.in.dataOut >> bridge.io.raw.dataOut

  // Engine ->
  val kv = new KeyValueEngine
  kv.io.ep <> multiData.io.out(0)
  kv.io.allocs <> allocator.io.allocs

  // dma -> LegoMemVirtualDMA
  val dma = new LegoMemDMA(config.dmaConfig, 40)
  dma.io.lego <> multiData.io.out(1)
  dma.io.dma <> kv.io.dma
  dma.io.regs.pid <> controller.io.regs.pid
  dma.io.regs.baseAddr <> controller.io.regs.baseAddr

  addPrePopTask(renameIO)
}

class KeyValueEngine(implicit config : KeyValueConfig) extends Component {
  // One -> multiple
  // Queue for multiple instances
  //

  // Fetch the first bucket
  // Fetch the second bucket
  val io = new Bundle {
    val ep = slave (LegoMemRawDataInterface())
    val dma = master (AxiStreamDMAInterface(config.dmaConfig))
    val allocs = master (config.allocInterfaceType)
  }

  val parser = new PacketParser(512, KeyValueHeader())
  parser.io.dataIn << io.ep.dataIn

  val builder = new PacketBuilder(512, KeyValueReplyHeader())
  builder.io.dataOut >> io.ep.dataOut

  // connect the modules
  val lock = new KeyValueStore
  lock.io.lego.dataIn <-< parser.io.dataOut
  lock.io.lego.headerIn <-< parser.io.headerOut
  lock.io.lego.dataOut >> builder.io.dataIn
  lock.io.lego.headerOut >> builder.io.headerIn

  // We can do this because we have single issuer
  // replace issuer here
  val issuer = new KeyValueIssuer
  issuer.io.request.header << lock.io.issue.bucketOut.header.stage()
  issuer.io.request.data << lock.io.issue.bucketOut.data.stage()

  // contains multiple issuer
  val executionUnit = new KeyValueExecutionUnit
  executionUnit.io.req <> issuer.io.issue
  executionUnit.io.loopback.header.stage()  >> issuer.io.loopback.header
  executionUnit.io.loopback.data.stage()  >> issuer.io.loopback.data
  executionUnit.io.allocs <> io.allocs
  executionUnit.io.res <> lock.io.issue.replyIn

  // DMA merge and arbitration here, including DMAs from execution unit and local ports
  val readArbiter = new AxiStreamDMAReadArbiter(2, config.dmaConfig)
  readArbiter.io.reads(0) <> executionUnit.io.dma.read
  readArbiter.io.reads(1) <> issuer.io.dma

  io.dma.read <> readArbiter.io.dma
  io.dma.write <> executionUnit.io.dma.write

  // Debug
}
