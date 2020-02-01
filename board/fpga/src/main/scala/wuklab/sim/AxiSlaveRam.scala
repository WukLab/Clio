package wuklab.sim

import spinal.core._
import spinal.lib
import spinal.lib._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config, Axi4R}
import spinal.lib.fsm._
import wuklab.ReturnStream
import wuklab.Utils._

// This class is only for simulation use
class AxiSlaveBRam(config : Axi4Config) extends Component {
  // parameters
  val offsetBits = log2Up(config.dataWidth)

  val io = slave (new Axi4(config))

  // We do not check the boundary
  val mem = new Mem(Bits(config.dataWidth bits), 1024)

  val readCtrl = new StateMachine {
    val idle = new State with EntryPoint
    val read = new State

    val curCmd = Reg(io.readCmd.payloadType)

    val ramRdCmd = Stream(UInt(10 bits))
    val ramRdRsp = mem.streamReadSync(ramRdCmd)

    ramRdCmd << ReturnStream(curCmd, isActive(read)).fmap(cmd => {
      cmd.addr(config.addressWidth - 1 downto offsetBits)
    })

    io.readRsp << ramRdRsp.fmap(data => {
      val rsp = new Axi4R(config)
      rsp.data := data
      rsp.id := curCmd.id
      rsp.user := curCmd.user
      rsp.setOKAY()
      rsp
    })

    io.readCmd.ready := isActive(idle)
    idle.whenIsActive {
      when (io.readCmd.fire) {
        io.readCmd.ready := True
        curCmd := io.readCmd.payload
        goto(read)
      }
    }

    read.whenIsActive {
      when (ramRdCmd.fire) {
        // When is fixed, we just ignore.
        when (curCmd.isINCR()) {
          // TODO: deal with align
          curCmd.addr := curCmd.addr + config.dataWidth / 8
        }
        when (curCmd.len === 0) {
          goto(idle)
        }
        curCmd.len := curCmd.len - 1
      }
    }
  }

  val writeCtrl = new StateMachine {
    val idle = new State with EntryPoint
    val trans = new State
    val resp = new State

    val curCmd = Reg(io.writeCmd.payloadType)

    val ramWrCmd = Stream(UInt(10 bits))
    // Address and Data
    mem.writePort << (ReturnStream(curCmd, isActive(trans)) >*< io.writeData).fmap(p => {
      val cmd = MemWriteCmd(mem)
      cmd.address := p.fst.addr
      cmd.data := p.snd.data
      cmd
    }).toFlow

    io.writeCmd.ready := isActive(idle)
    io.writeRsp.valid := isActive(resp)

    idle.whenIsActive {
      when (io.writeCmd.fire) {
        curCmd := io.writeCmd.payload
        goto (trans)
      }
    }

    trans.whenIsActive {

      when (curCmd.len === 0) {
        // This is a slow impl, take an extra cycle, but its okay in simulation
        goto(resp)
      }
    }

    resp.whenIsActive {

    }

  }

  // When move out, push to a fifo; (this move out blocked by the ready!)
  // when in state, accept one beat.
  // when in state, keep trans until

}