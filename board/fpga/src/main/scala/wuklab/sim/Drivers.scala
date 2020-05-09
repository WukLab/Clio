package wuklab.sim

import java.math.BigInteger

import scodec.bits
import wuklab._
import spinal.core._
import spinal.lib._
import spinal.core.sim._
import spinal.lib.bus.amba4.axi._
import spinal.lib.bus.amba4.axilite._
import spinal.sim.SimThread

import scala.collection.{SortedMap, mutable}
import scala.util.Random
import scala.collection.mutable.ArrayBuffer

// TODO: driver, drivable
trait SimulationService {
  val clockDomain : ClockDomain
  val waitInit = {
    waitUntil(clockDomain.isResetDeasserted)
    clockDomain.waitRisingEdge(3)
  }
}

object AssignmentFunctions {
  implicit def uintAssign(a : Int, t : UInt) = t #= a
  implicit def RdReqAssign(key : Int, t : LookupReadReq) = { t.key #= key }
  implicit def WrReqAssign(a : (Int, Int, Int, Boolean), t : LookupWrite) = {
    val (key, mask, value, enable) = a
    t.key #= key
    t.mask #= mask
    t.value #= value
    t.enable #= enable
  }
  implicit def WrReqAssign(a : (Int, Int, Int, Boolean, Int), t : LookupWrite) = {
    assert(t.useUsed)
    val (key, mask, value, used, usedCell) = a
    t.key #= key
    t.mask #= mask
    t.value #= value
    t.used #= used
    t.usedCell #= usedCell
  }
  implicit def WrReqAssign(a : (Int, Int, Int), t : LookupWrite) = {
    val (key, mask, value) = a
    t.key #= key
    t.mask #= mask
    t.value #= value
    if (t.useUsed) t.used #= false
  }
  implicit def AddressLookupRequestAssign(a : (Int, Int, Int), t : AddressLookupRequest): Unit = {
    val (seq, reqType, tag) = a
    t.seqId #= seq
    t.reqType #= reqType
    t.tag #= tag
  }
  implicit def PageTableEntryAssign(a : (Int, Int, Boolean, Boolean, Int), pte : PageTableEntry): Unit = {
    val (tag, ppa, valid, used, pteAddr) = a
    if (pte.useTag) pte.tag #= tag
    if (pte.usePpa) pte.ppa #= ppa
    pte.allocated #= valid
    pte.used #= used
    if (pte.usePteAddr) pte.pteAddr #= pteAddr
  }

  implicit def ControlRequestAssign(a : (Int, Int, Int), cmd : ControlRequest): Unit = {
    val (cid, cmdId, param) = a
    cmd.addr #= 0
    cmd.epid #= cid
    cmd.cmd #= cmdId
    cmd.param32 #= param & 0xFFFFFFFF
    cmd.param8 #= param >> 32
  }

  implicit def ControlRequestToBitsAssign(a : (BigInt, BigInt, BigInt), bits : Bits): Unit = {
    val (cid, cmd, param) = a
    bits #= (cid << 56) | (cmd << 44) | param
  }

  def joinAll( simThreads: SimThread *): Unit = {
    simThreads.foreach(_.join())
  }
}

trait Testable[T <: Data, A] {
  def assign(a : A, t : T) : Unit
}

object Testable {

//  TODO: make this a type class
//  see https://scalac.io/typeclasses-in-scala/
//
//  This is a syntax sugar, we must unfold this by ourselves.
//  def apply[A](implicit sh: Show[A]): Show[A] = sh
//
//  def show[A: Show](a: A) = Show[A].show(a)
//
//  implicit class ShowOps[A: Show](a: A) {
//    def show = Show[A].show(a)
//  }

  implicit val Int2lookupReadReq : Testable[LookupReadReq, Int] = new Testable[LookupReadReq, Int] {
    override def assign(a: Int, t: LookupReadReq): Unit = { t.key #= a }
  }

}

trait Driver[T <: Data] {
  val wire : T
  def tik : Boolean
  def update(update : Boolean) : Unit
  def finish : Boolean
}

object SeqDataGen {
  def apply[T <: Data, A](seq : A *)(t : T)(implicit f : (A, T) => Unit): Driver[T] = new Driver[T] {
    var idx = 0
    val wire = t
    // @ret: the valid of the value
    def tik : Boolean = {
      println(f"At $idx ")
      if (idx < seq.size) {
        f(seq(idx), t)
        true
      } else
        false
    }

    def update(update : Boolean): Unit = {
      if (update) idx = idx + 1
    }

    def finish = idx >= seq.size
  }
}

class BitStreamDataGen(seqs : Seq[scodec.bits.BitVector] *)(frag : Stream[Fragment[Bits]]) extends Driver[Stream[Fragment[Bits]]] {
  override val wire = frag
  var idx = 0
  var offset = 0

  override def tik = {
    if (idx >= seqs.size)
      false
    else {
      val seq = seqs(idx)
      frag.payload.fragment #= BigInt(seq(offset).toByteArray)
      frag.payload.last #= (offset + 1) == seq.size
      true
    }
  }
  override def update(update: Boolean): Unit = {
    if (update) {
      offset = offset + 1
      if (offset == seqs(idx).size) {
        offset = 0
        idx = idx + 1
      }
    }
  }
  override def finish = idx >= seqs.size
}

object BitStreamDataGen {
  def apply(seqs: Seq[bits.BitVector]*)(frag: Stream[Fragment[Bits]]): BitStreamDataGen = new BitStreamDataGen(seqs : _*)(frag)
}

class BitAxisDataGen(bits : (scodec.bits.BitVector, Int))(frag : Fragment[AxiStreamPayload])
  extends Driver[Fragment[AxiStreamPayload]] {

  override val wire = frag
  val config = wire.fragment.config

  var tail = bits._1
  val dest = bits._2
  def data = tail.take(config.dataWidth).padRight(config.dataWidth).reverseByteOrder.toByteArray

  override def tik = {
    val isLast = tail.sizeLessThanOrEqual(config.dataWidth)
    wire.fragment.tdata #= BigInt(data)
    wire.last #= isLast
    if (config.useDest) wire.fragment.tdest #= dest
    if (config.useKeep) wire.fragment.tkeep #= {
      val shiftWidth = if (isLast) tail.bytes.size % config.keepWidth else config.keepWidth
//      println("DO TKEEP: %d -> %X" format (shiftWidth, (1L << shiftWidth) - 1))
      (1L << shiftWidth) - 1
    }
    true
  }
  override def update(update: Boolean): Unit = {
    if (update) {
      println(s"Axis: Send Data<${config.dataWidth}>: ${data.map("%02X" format _).mkString("_")}")
      tail = tail.drop(config.dataWidth)
    }
  }
  override def finish = tail.isEmpty
}

object BitAxisDataGen {
  def apply(seqs: bits.BitVector)(frag: Fragment[AxiStreamPayload]): BitAxisDataGen =
    new BitAxisDataGen((seqs, 0))(frag)
  def apply(seqs: bits.BitVector, port : Int)(frag: Fragment[AxiStreamPayload]): BitAxisDataGen =
    new BitAxisDataGen((seqs, port))(frag)
}

class StreamMonitor[T <: Data](stream : Stream[T], clockDomain: ClockDomain) {
  stream.ready #= true

  // Monitor the data

}

object StreamHalt{
  def apply[T <: Data](stream : Stream[T]) {
    if (stream.isMasterInterface) {
      stream.ready #= false
    } else {
      stream.valid #= false
    }
  }
}

class StreamDriver[T <: Data](stream : Stream[T], clockDomain: ClockDomain) {

  stream.valid #= false
  stream.payload.flatten.filter(p => p != null).foreach {
    case uint: UInt => uint #= 0
    case bits: Bits => bits #= 0
    case bool: Bool => bool #= false
  }
  println (s"DRIVER: build for ${stream.getName()}")

  def #= (gen : T => Driver[T]) = {
    val driver = gen(stream.payload)

    while (!driver.finish) {
      stream.valid #= driver.tik
      clockDomain.waitFallingEdge
      driver.update(stream.ready.toBoolean && stream.valid.toBoolean)
      clockDomain.waitRisingEdge
    }
    stream.valid #= false

  }

}

class FlowDriver[T <: Data](flow : Flow[T], clockDomain: ClockDomain) {

  flow.valid #= false
  flow.payload.flatten.filter(p => p != null).foreach {
    case uint: UInt => uint #= 0
    case bits: Bits => bits #= 0
    case bool: Bool => bool #= false
  }

  def #= (gen : T => Driver[T]) = {
    val driver = gen(flow.payload)

    while (!driver.finish) {
      flow.valid #= driver.tik
      driver.update(true)
      clockDomain.waitRisingEdge(1)
    }
    flow.valid #= false
  }

}

class AxiLite4RegDriver (bus: AxiLite4, clockDomain: ClockDomain) {

  assert(!bus.isMasterInterface)

  bus.ar.valid #= false
  bus.ar.prot #= 0 // 0 for no prot
  bus.aw.valid #= false
  bus.aw.prot #= 0 // 0 for no prot
  bus.w.valid #= false
  bus.b.ready #= true
  bus.r.ready #= true

  def write(addr : Int, value : Int) = {
    bus.aw.valid #= true
    bus.aw.addr #= addr
    bus.w.valid #= true
    bus.w.data #= value
    clockDomain.waitRisingEdge
    bus.aw.valid #= false
    bus.w.valid #= false
  }

  def read(addr : Int) = {
    bus.ar.valid #= true
    bus.ar.addr #= addr
    clockDomain.waitRisingEdge
    bus.ar.valid #= false
  }
}

class Axi4MasterCommandDriver() {

}

abstract class Axi4SlaveMemoryDriver (clockDomain: ClockDomain) extends SimulationService {

  case class ChannelStates(
                            var state : Int = 0,
                            var beats : Int = 0,
                            var burst : Int = 0,
                            var addr : BigInt = 0
                          )
  def memoryRead(addr : BigInt, size : Int) : BigInt
  def memoryWrite(addr : BigInt, size : Int, data : BigInt) : Unit
  def init(values : (BigInt, Seq[Byte]) *): Axi4SlaveMemoryDriver

  def initValue[T](values : (BigInt, T) *)(implicit assign: T => Seq[Byte]): Axi4SlaveMemoryDriver = {
    init(values.map(p => (p._1, assign(p._2))) : _*)
  }

  def handleRead (config : Axi4Config, cmd : Stream[Axi4Ar], rsp : Stream[Axi4R]) : Unit = {
    val read = ChannelStates()
    // We do not join this thread, since it is a service not a simulating components
    fork {
      cmd.ready #= false
      rsp.valid #= false
      waitInit
      while (true) {
        // State action
        cmd.ready #= false
        rsp.valid #= false
        read.state match {
          case 0 => {
            cmd.ready #= true
          }
          case 1 => {
            if (config.useResp)
              rsp.resp #= 0
            rsp.data #= memoryRead(read.addr, config.bytePerWord)
            rsp.valid #= true
          }
        }

        clockDomain.waitFallingEdge

        // State trans
        read.state match {
          case 0 => {
            if (cmd.valid.toBoolean) {
              read.beats = if (config.useLen) cmd.len.toInt else 0
              read.burst = if (config.useBurst) cmd.burst.toInt else 0
              read.addr = cmd.addr.toBigInt
              read.state = 1
            }
          }
          case 1 => {
            if (rsp.ready.toBoolean) {
              if (read.beats == 0) read.state = 0
              if (read.burst != 0)
                read.addr += config.bytePerWord
              read.beats -= 1
            }
          }
        }

        clockDomain.waitRisingEdge
      }
    }
  }

  def handleWrite (config : Axi4Config, cmd : Stream[Axi4Aw], data : Stream[Axi4W], rsp : Stream[Axi4B]) : Unit = {
    val write = ChannelStates()
    // We do not join this thread, since it is a service not a simulating components
    fork {
      // init value
      cmd.ready #= false
      data.ready #= false
      rsp.valid #= false
      waitInit
      while (true) {
        // Data assignment
        cmd.ready #= false
        data.ready #= false
        rsp.valid #= false
        write.state match {
          case 0 => {
            cmd.ready #= true
          }
          case 1 => {
            data.ready #= true
          }
          case 2 => {
            rsp.valid #= true
            if (config.useResp) rsp.resp #= 0
          }
        }

        clockDomain.waitFallingEdge

        // State transfer
        write.state match {
          case 0 => {
            if (cmd.valid.toBoolean) {
              write.beats = if (config.useLen) cmd.len.toInt else 0
              write.burst = if (config.useBurst) cmd.burst.toInt else 0
              write.addr = cmd.addr.toBigInt
              write.state = 1
            }
          }
          case 1 => {
            if (data.valid.toBoolean) {
              if (write.beats == 0) write.state = 2
              memoryWrite(write.addr, config.bytePerWord, data.data.toBigInt)
              write.beats -= 1
              if (write.burst != 0)
                write.addr += config.bytePerWord
            }
          }
          case 2 => {
            if (rsp.ready.toBoolean) write.state = 0
          }
        }

        clockDomain.waitRisingEdge(1)
      }
    }
  }

  def =# (bus : Axi4WriteOnly): Unit = {
    handleWrite(bus.config, bus.writeCmd, bus.writeData, bus.writeRsp)
  }
  def =# (bus : Axi4ReadOnly): Unit = {
    handleRead(bus.config, bus.readCmd, bus.readRsp)
  }
  def =# (bus : Axi4) : Unit = {
    handleWrite(bus.config, bus.writeCmd, bus.writeData, bus.writeRsp)
    handleRead(bus.config, bus.readCmd, bus.readRsp)
  }

}

class ArrayMemoryDriver(val clockDomain: ClockDomain, size : Int) extends Axi4SlaveMemoryDriver(clockDomain) {
  private val memory : ArrayBuffer[Byte] = ArrayBuffer.fill(size)(0)

  override def memoryRead(addr : BigInt, size : Int) : BigInt = {
    println(f"Memory Read @$addr%08X:$size")
    // TODO: deal with unaligned accesses
    val buffer = new Array[Byte](size)
    for (idx <- 0 until size)
      buffer(idx) = memory(addr.toInt + idx)
    new BigInt(new BigInteger(buffer.reverse))
  }

  override def memoryWrite(addr : BigInt, size : Int, data : BigInt) : Unit = {
    println(f"Memory Write @$addr%08X:$size")
    val buffer = data.toByteArray
    for (idx <- 0 until Math.min(size, buffer.size)) {
      memory(idx + addr.toInt) = buffer(idx)
    }
  }

  override def init(values : (BigInt, Seq[Byte]) *): Axi4SlaveMemoryDriver = {
    for ((addr, data) <- values) {
      println(f"Memory Init @$addr%08X:${data.size}")
      for (idx <- 0 until data.size) {
        memory(addr.toInt + idx) = data(idx)
      }
    }
    this
  }
}

class DictMemoryDriver(val clockDomain: ClockDomain) extends Axi4SlaveMemoryDriver(clockDomain) {

  private val memory : mutable.HashMap[BigInt, Seq[Byte]] = mutable.HashMap()

  // TODO: Just an initial implementation. we only allow a whole segment
  override def memoryRead(addr: BigInt, size: Int): BigInt = {
    println(f"Memory Read @$addr%08X:$size")
    val resultArray = memory
      .find { case (s, seq) => addr >= s && addr < s + seq.size }
      .map { case (s, seq) => seq.slice((addr - s).toInt, (addr - s + size).toInt).toArray }
      .getOrElse { println("Memory Read Invalid!"); Array.fill(size)(0 : Byte) }
    new BigInt(new BigInteger(resultArray.reverse))
  }

  override def memoryWrite(addr: BigInt, size: Int, data: BigInt): Unit = {
    println(f"Memory Write @$addr%08X:$size")
    memory += addr -> data.toByteArray.toSeq
  }

  override def init(values: (BigInt, Seq[Byte])*) = {
    memory ++= values
    this
  }
}

class LegoMemEndPointInterconnect (val clockDomain: ClockDomain) extends SimulationService {

  // array for current
  type ValidSelection[T <: Data] = T => Bool
  type ReadySelection[T <: Data] = T => Bool
  type TDestSelection[T <: Data] = T => UInt

  val decision = mutable.Set[(Int, Int)]()
  val ctrlDecision = mutable.Set[(Int, Int)]()
  val eps = mutable.Map[Int, LegoMemEndPoint]()

  def validPort(port : Int) = eps.map(_._1).toSeq.contains(port)
  def halt(): Unit = eps.foreach { case (_, ep) =>
    ep.dataIn.valid  #= false
    ep.dataOut.ready #= false
    ep.ctrlIn.valid  #= false
    ep.ctrlOut.ready #= false
  }

  def clear = {
    halt()
    decision.clear()
    ctrlDecision.clear()
  }


  // DO assignment
  def tik() = {
    // We first halt on all ports
    halt()
    // Then we connect our selected ports
    for ((src, dst) <- decision) {
      if (validPort(dst) && validPort(src)) {

        val out = eps(src).dataOut
        val in  = eps(dst).dataIn

        // TODO: revoke ready
        out.ready #= in.ready.toBoolean & !in.last.toBoolean
        in.valid #= out.valid.toBoolean
        in.tdata #= out.tdata.toBigInt
        in.tdest #= out.tdest.toBigInt
        in.last #= out.last.toBoolean
      }
    }
    for ((src, dst) <- ctrlDecision) {
      if (validPort(dst) && validPort(src)) {

        val out = eps(src).ctrlOut
        val in  = eps(dst).ctrlIn

        // TODO: revoke ready
        out.ready #= in.ready.toBoolean
        in.valid #= out.valid.toBoolean
        in.tdata #= out.tdata.toBigInt
        in.tdest #= out.tdest.toBigInt
      }
    }
  }

  def forward(): Unit = {
    fork {
      // Update status
      while (true) {

        if (clockDomain.isResetAsserted) {
          clear
          waitUntil(clockDomain.isResetDeasserted)
        } else {

          clockDomain.waitFallingEdge

          // get new ones
          // ** For Data **
          val routingInfo = eps.toSeq
            .filter { case (_, axis) => axis.dataOut.valid.toBoolean }
            .filter { case (_, axis) => !decision.map(_._2).contains(axis.dataOut.tdest.toInt) }
            .map { case (port, axis) => (port, axis.dataOut.tdest.toInt) }

          if (routingInfo.size > 0)
            println(s"DATA ROUTING INFO: ${pprint.apply(routingInfo)}")

          // update decisions
          val routingTable = routingInfo.groupBy(_._2).mapValues(_.map(_._1))
          decision ++= routingTable.mapValues(xs => xs(Random.nextInt(xs.length)))
            .map(_.swap)
            .filter { case (_, dest) => validPort(dest) }

          // ** for Ctrl **
          val ctrlRoutingInfo = eps.toSeq
            .filter { case (_, axis) => axis.ctrlOut.valid.toBoolean }
            .filter { case (_, axis) => !decision.map(_._2).contains(axis.ctrlOut.tdest.toInt) }
            .map { case (port, axis) => (port, axis.ctrlOut.tdest.toInt) }

          if (routingInfo.size > 0)
            println(s"CTRL ROUTING INFO: ${pprint.apply(routingInfo)}")

          // update decisions
          val ctrlRoutingTable = routingInfo.groupBy(_._2).mapValues(_.map(_._1))
          ctrlDecision ++= ctrlRoutingTable.mapValues(xs => xs(Random.nextInt(xs.length)))
            .map(_.swap)
            .filter { case (_, dest) => validPort(dest) }


          clockDomain.waitRisingEdge

          tik()

          // Clean up last ones
          eps.foreach { case (port, axis) =>
            // TODO: this remove cause extra cycles
            val out = axis.dataOut
            if (out.valid.toBoolean && out.ready.toBoolean && out.last.toBoolean)
              decision.remove((port, out.tdest.toInt))

            val ctrl = axis.ctrlOut
            // Control do one cycle transfer
            if (ctrl.valid.toBoolean && ctrl.ready.toBoolean)
              ctrlDecision.remove((port, ctrl.tdest.toInt))
          }

          if (decision.size > 0)
            println(s"ROUTING DECISION: ${pprint.apply(decision)}")

        }

      }

    }

  }

  def =# (port : Int)(ep : LegoMemEndPoint): Unit = eps += port -> ep

  // initilization
  forward()
}
