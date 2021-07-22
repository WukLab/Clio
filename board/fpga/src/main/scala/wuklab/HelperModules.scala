package wuklab

import spinal.core._
import spinal.lib._

class StreamCounter extends Component {
  val io = new Bundle {
    val validIn = in Bool
    val readyIn = in Bool
    val lastIn = in Bool

    val validOut = in Bool
    val readyOut = in Bool
    val lastOut = in Bool

    val firstInTimestamp = out UInt(32 bits)
    val lastInTimestamp = out UInt(32 bits)
    val firstOutTimestamp = out UInt(32 bits)
    val lastOutTimestamp = out UInt(32 bits)
  }

  val timer = Counter(32 bits, True)
  val firstInTimestamp  = Reg (UInt(32 bits)) init 0xdeadbeef
  val lastInTimestamp   = Reg (UInt(32 bits)) init 0xdeadbeef
  val firstOutTimestamp = Reg (UInt(32 bits)) init 0xdeadbeef
  val lastOutTimestamp  = Reg (UInt(32 bits)) init 0xdeadbeef

  io.firstInTimestamp := firstInTimestamp
  io.lastInTimestamp := lastInTimestamp
  io.firstOutTimestamp := firstOutTimestamp
  io.lastOutTimestamp := lastOutTimestamp

  val firstIn = Reg (Bool) init True
  val firstOut = Reg (Bool) init True

  when (io.validIn && io.readyIn) {
    firstIn := False

    when (firstIn) {
      firstInTimestamp := timer.value
    }

    when (io.lastIn) {
      lastInTimestamp := timer.value
      firstIn := True
    }
  }

  when (io.validOut && io.readyOut) {
    firstOut := False

    when (firstOut) {
      firstOutTimestamp := timer.value
    }

    when (io.lastOut) {
      firstOutTimestamp := timer.value
      firstOut := True
    }
  }

}