package wuklab.sim

import java.math.BigInteger

import wuklab._
import spinal.core._
import spinal.lib._
import spinal.core.sim._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config, Axi4ReadOnly, Axi4WriteOnly}
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Ax, AxiLite4ReadOnly}
import spinal.sim.SimThread

import scala.collection.mutable.ArrayBuffer

// TODO: driver, drivable

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
  implicit def AddressLookupRequestAssign(a : (Int, Int, Int, Int), t : AddressLookupRequest): Unit = {
    val (pid, seq, reqType, va) = a
    t.pid #= pid
    t.seqId #= seq
    t.reqType #= reqType
    t.va #= va
  }
  implicit def PageTableEntryAssign(a : (Int, Int, Boolean, Boolean, Int), pte : PageTableEntry): Unit = {
    val (tag, ppa, valid, used, pteAddr) = a
    if (pte.useTag) pte.tag #= tag
    if (pte.usePpa) pte.ppa #= ppa
    pte.allocated #= valid
    pte.used #= used
    if (pte.usePteAddr) pte.pteAddr #= pteAddr
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

class StreamDriver[T <: Data](stream : Stream[T], clockDomain: ClockDomain) {

  stream.valid #= false
  stream.payload.flatten.filter(p => p != null).foreach {
    case uint: UInt => uint #= 0
    case bits: Bits => bits #= 0
    case bool: Bool => bool #= false
  }

  def #= (gen : T => Driver[T]) = {
    val driver = gen(stream.payload)

    while (!driver.finish) {
      stream.valid #= driver.tik
      clockDomain.waitFallingEdge
      driver.update(stream.ready.toBoolean)
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

class Axi4SlaveMemoryDriver (clockDomain: ClockDomain, size : Int) {

  case class ChannelStates(
                            var state : Int = 0,
                            var beats : Int = 0,
                            var burst : Int = 0,
                            var addr : Int = 0
                          )

  private val memory : ArrayBuffer[Byte] = ArrayBuffer.fill(size)(0)
  private val read = ChannelStates()
  private val write = ChannelStates()

  def memoryRead(addr : Int, size : Int) : BigInt = {
    // TODO: deal with unaligned accesses
    val buffer = new Array[Byte](size)
    for (idx <- 0 until size)
      buffer(idx) = memory(addr + idx)
    new BigInt(new BigInteger(buffer))
  }

  def memoryWrite(addr : Int, size : Int, data : BigInt) : Unit = {
    val buffer = data.toByteArray
    for (idx <- 0 until Math.min(size, buffer.size)) {
      memory(idx + addr) = buffer(idx)
    }
  }

  def init(values : (Int, Seq[Byte]) *) = {
    for ((addr, data) <- values) {
      for (idx <- 0 until data.size) {
        memory(addr + idx) = data(idx)
      }
    }
    this
  }

  def =# (bus : Axi4ReadOnly) : Unit = {
    val config = bus.config
    // We do not join this thread, since it is a service not a simulating components
    fork {
      bus.readCmd.ready #= false
      bus.readRsp.valid #= false
      waitUntil(clockDomain.isResetDeasserted)
      while (true) {
        // State action
        bus.readCmd.ready #= false
        bus.readRsp.valid #= false
        read.state match {
          case 0 => {
            bus.readCmd.ready #= true
          }
          case 1 => {
            if (config.useResp)
              bus.readRsp.resp #= 0
            bus.readRsp.data #= memoryRead(read.addr, config.bytePerWord)
            bus.readRsp.valid #= true
          }
        }

        clockDomain.waitFallingEdge

        // State trans
        read.state match {
          case 0 => {
            if (bus.readCmd.valid.toBoolean) {
              read.beats = if (config.useLen) bus.readCmd.len.toInt else 0
              read.burst = if (config.useBurst) bus.readCmd.burst.toInt else 0
              read.addr = bus.readCmd.addr.toInt * config.bytePerWord
              read.state = 1
            }
          }
          case 1 => {
            if (bus.readRsp.ready.toBoolean) {
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

  def =# (bus : Axi4WriteOnly) : Unit = {
    val config = bus.config
    // We do not join this thread, since it is a service not a simulating components
    fork {
      // init value
      bus.writeCmd.ready #= false
      bus.writeData.ready #= false
      bus.writeRsp.valid #= false
      waitUntil(clockDomain.isResetDeasserted)
      while (true) {
        // Data assignment
        bus.writeCmd.ready #= false
        bus.writeData.ready #= false
        bus.writeRsp.valid #= false
        write.state match {
          case 0 => {
            bus.writeCmd.ready #= true
          }
          case 1 => {
            bus.writeData.ready #= true
          }
          case 2 => {
            bus.writeRsp.valid #= true
            bus.writeRsp.resp #= 0
          }
        }

        clockDomain.waitFallingEdge

        // State transfer
        write.state match {
          case 0 => {
            if (bus.writeCmd.valid.toBoolean) {
              write.beats = if (config.useLen) bus.writeCmd.len.toInt else 0
              write.burst = if (config.useBurst) bus.writeCmd.burst.toInt else 0
              write.addr = bus.writeCmd.addr.toInt * config.bytePerWord
              write.state = 1
            }
          }
          case 1 => {
            if (bus.writeData.valid.toBoolean) {
              if (write.beats == 0) write.state = 2
              memoryWrite(write.addr, config.bytePerWord, bus.writeData.data.toBigInt)
              write.beats -= 1
              if (write.burst != 0)
                write.addr += config.bytePerWord
            }
          }
          case 2 => {
            if (bus.writeRsp.ready.toBoolean) write.state = 0
          }
        }

        clockDomain.waitRisingEdge(1)
      }
    }
  }

  def =# (bus : Axi4) : Unit = {
    =# (bus.toWriteOnly())
    =# (bus.toReadOnly())
  }

}

