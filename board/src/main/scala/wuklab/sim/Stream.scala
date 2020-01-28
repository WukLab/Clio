package wuklab.sim

import java.math.BigInteger

import wuklab._
import spinal.core._
import spinal.lib._
import spinal.core.sim._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config}
import spinal.sim.SimThread

import scala.collection.mutable.ArrayBuffer

object AssignmentFunctions {
  implicit def uintAssign(a : Int, t : UInt) = t #= a

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
  def tik(update : Boolean) : Boolean
  def finish : Boolean
}

object SeqDataGen {
  def apply[T <: Data, A](seq : A *)(t : T)(implicit f : (A, T) => Unit): Driver[T] = new Driver[T] {
    var idx = 0
    val wire = t
    // @ret: the valid of the value
    def tik(update : Boolean) : Boolean = {
      println(f"At $idx : $update, ")
      f(seq(idx), t)
      idx = idx + 1
      true
    }

    def finish = idx >= seq.size
  }
}

class StreamDriver[T <: Data](stream : Stream[T], clockDomain: ClockDomain) {

  stream.valid #= false

  def #= (gen : T => Driver[T]) = {
    val driver = gen(stream.payload)

    while (!driver.finish) {
      stream.valid #= driver.tik(stream.ready.toBoolean)
      clockDomain.waitRisingEdge(1)
    }
    stream.valid #= false

  }

}

class FlowDriver[T <: Data](flow : Flow[T], clockDomain: ClockDomain) {

  flow.valid #= false

  def #= (gen : T => Driver[T]) = {
    val driver = gen(flow.payload)

    while (!driver.finish) {
      flow.valid #= driver.tik(true)
      clockDomain.waitRisingEdge(1)
    }
    flow.valid #= false
  }

}

class Axi4SlaveMemoryDriver (bus : Axi4, clockDomain: ClockDomain, size : Int) {

  assert(!bus.isMasterInterface)
  val config = bus.config
  private val memory : ArrayBuffer[Byte] = new ArrayBuffer[Byte](size)
  private var readState : Int = 0
  private var readBeats : Int = 0
  private var readAddr : Int = 0

  private var writeState : Int = 0
  private var writeBeats : Int = 0

  def memoryRead(addr : Int, size : Int) : BigInt = {
    val buffer = new Array[Byte](size)
    memory.copyToArray(buffer, addr, size)
    new BigInt(new BigInteger(buffer))
  }

  def memoryWrite(addr : Int, size : Int, data : BigInt) : Unit = {
    val buffer = data.toByteArray
    for (idx <- 0 until size) {
      memory(idx + addr) = buffer(idx)
    }
  }

  def init() = {

  }
  def run() = {
    fork {
      // implement a scala emulator here
      while (true) {
        // Read path
        // Default values
        bus.readCmd.ready #= false
        bus.readRsp.valid #= false
        readState match {
          case 0 => {
            bus.readCmd.ready #= true
            if (bus.readCmd.fire.toBoolean) {
              readBeats = bus.readCmd.len.toInt
              readAddr = bus.readCmd.addr.toInt
            }
          }
          case 1 => {
            if (readBeats == 0) readState = 1
            bus.readRsp.resp #= 0 // 0 for OK
            bus.readRsp.data #= memoryRead(readAddr, config.dataWidth / 8)
            if (bus.readRsp.fire.toBoolean) {
              // Config Burst here
            }
          }
        }
        // Write path
        

        // Next cycle
        clockDomain.waitRisingEdge(1)
      }
    }
  }

}
