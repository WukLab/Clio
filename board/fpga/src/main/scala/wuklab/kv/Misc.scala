package wuklab.kv

import spinal.core._
import spinal.lib._
import wuklab._

import Utils._


class FixedHeaderParser(
                         val dataWidth : Int, val sizeWidth : Int, val headerWidth : Int
                       ) extends Component with HeaderProcessor {

  val io = new Bundle {
    val dataIn = slave Stream Fragment(Bits(dataWidth bits))
    val dataOut = master Stream Fragment(Bits(dataWidth bits))
    val headerOut = master Stream Bits(headerWidth bits)
    val size = slave Stream UInt(sizeWidth bits)
  }

  val (toHeader, toDataF) = StreamFork2(io.dataIn)
  // 1. easy path: header
  io.headerOut << toHeader.takeFirst.toStreamOfFragment.fmap (_(headerWidth-1 downto 0))

  // 2. data
  // 2.1 filter out no data headers
  val toData = toDataF.throwBy {f => toDataF.first && io.size.payload <= headerBytes }
  // 2.2 generate data using a state machine
  val headerIn = toData.fragment(headerWidth-1 downto 0)

  // Cycle one, we fill in reg

  val regValid = Reg (Bool) init False
  val size = Reg (UInt(sizeWidth bits))
  val lastCycleNoNeedData = size <= offsetBytes && size =/= 0

  val lastCycle = size <= dataBytes
  val dataReg = RegNextWhen(toData.fragment(dataWidth-1 downto headerWidth), toData.fire)
  val dataPad = Mux(regValid, dataReg, toData.fragment(dataWidth-1 downto headerWidth))
  val outData = toData.fragment(headerWidth-1 downto 0) ## dataPad

  io.dataOut.fragment := outData
  io.dataOut.last := lastCycle

  toData.ready := False
  io.dataOut.valid := False

  io.size.ready := toDataF.fire && toDataF.first && io.size.payload <= headerBytes

  // Let compiler optimize this shit
  when (!regValid) {
    // We need to fillin the reg!
    toData.ready := True

    size := io.size.payload - headerBytes

    when (toData.valid) {
      io.size.ready := True

      when (io.size.payload <= dataBytes && io.dataOut.ready) {
        io.dataOut.valid := True
        io.dataOut.last := True
      } otherwise {
        regValid := True
      }
    }
  } otherwise {
    // We can emit data
    // From here, the size IS pure data size

    when (lastCycle) {

      when (lastCycleNoNeedData) {
        io.dataOut.valid := True
        when (io.dataOut.ready) {
          regValid := False
          // TODO: optimization here, can be revoke
          // TODO: DO NOT WAIT TO ENTER STATE 0
          toData.ready := True
          size := io.size.payload - headerBytes

          // We can only do register here, can not directly send
          when (toData.valid && io.size.valid) {
            io.size.ready := True
            regValid := True
          }
        }
      } elsewhen (toData.valid) {
        io.dataOut.valid := True
        when (io.dataOut.ready) {
          toData.ready := True
          regValid := False
        }
      }

    } elsewhen (toData.valid) {
      io.dataOut.valid := True
      when (io.dataOut.ready) {
        toData.ready := True
        size := size - dataBytes
      }
    }
  }
}
