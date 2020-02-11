package wuklab

import spinal.core._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config}
import spinal.lib._
import spinal.lib.fsm._

import Utils._

class MemoryAccessUnit(implicit config : CoreMemConfig) extends Component {
  val lookbackSize = 256

  val io = new Bundle {
    // interface from request
    val req = slave  Stream PageTableEntry(useTag = false, ppaWidth = 24)
    val dataIn  = slave Stream UInt(512 bits)
    val dataOut = master Stream UInt(512 bits)

    // Interface to the data mover
    val wrCmd = master Stream DataMoverCmd(config.physicalAddrWidth, true)
    val rdCmd = master Stream DataMoverCmd(config.physicalAddrWidth, true)
    val dataWr = master Stream UInt(512 bits)
    val dataRd = slave Stream UInt(512 bits)
  }


  val header = io.dataIn.payload.as(LegoMemHeader(48))
  val lookback = StreamFifo(UInt(512 bits), lookbackSize)

  // State machine here
  // TODO: we can split this FSM into 2 fsms, this one push things into the loopback
  // This one send out commands, parse heads, forward things
  val inputFsm = new StateMachine {
    val init= new State with EntryPoint
    val busyWrite = new State

    val counter = UInt(8 bits)

    val Seq(headerIfc, dataIfc) = isActive(busyWrite).demux(io.dataIn)
    dataIfc >> io.dataWr
    // Generate header
    headerIfc >> lookback.io.push
    // repl



    // a.stage.translateInto(checked instruction)
    // OR, in -> a, f(a), a -> b

    // Commands can be generated outside of the state machine. just need
    init.whenIsActive {
      when (io.dataIn.fire) {
        switch (header.reqType) {
          is (MemoryRequestType.write) {
            goto (busyWrite)
            // generate commands
          }
        }
      }
    }

    busyWrite.whenIsActive {
      when (io.dataIn.fire) {
        when (counter === 1) { goto (init) }
        counter := counter - 1
      }
    }
  }

  // This FSM pull things out of loopback and dataFifo. This is simple!
  val outputCtrl = new StateMachine {
    val init= new State with EntryPoint
    val busyRead = new State
    io.dataOut << isActive(busyRead).select(lookback.io.pop, io.dataRd)

  }

  def getSeqIdFromHeader(header: LegoMemHeader): UInt = {
    header.seqId
  }
  def getBeatsHeader(header: LegoMemHeader): UInt = {
    header.size
  }

  //  def commandCheck() : Bool = {
  //
  //  }
}
