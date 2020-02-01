package wuklab

import wuklab.Utils._
import spinal.core._
import spinal.lib._

class IDPool(numIds: Int) extends Component {
  require (numIds > 0)
  val idWidth = log2Up(numIds)

  val io = new Bundle {
    val free  = slave  Flow UInt(idWidth bits)
    val alloc = master Stream UInt(idWidth bits)
  }

  // True indicates that the id is available
  val bitmap = Reg(UInt(numIds bits)) init UInt(numIds bits).setAll()
  val select = Reg(UInt(idWidth bits)) init 0
  val valid  = Reg(Bool) init True

  io.alloc.valid   := valid
  io.alloc.payload := select

  val taken   = io.alloc.ready.asUInt(numIds bits) |<< io.alloc.payload
  val given   = io.free.valid.asUInt(numIds bits)  |<< io.free.payload
  val bitmap1 = (bitmap & ~taken) | given
  val select1 = OHToUInt(~leftOR(bitmap1, numIds) & bitmap1)
  val valid1  = bitmap1.orR

  // Clock gate the bitmap
  when (io.alloc.ready || io.free.valid) {
    bitmap := bitmap1
    valid  := valid1
  }

  // Make select irrevocable
  when (io.alloc.ready || (!io.alloc.valid && io.free.valid)) {
    select := select1
  }

  // No double freeing
  assert (!io.free.valid || !(bitmap & ~taken)(io.free.payload))
}
