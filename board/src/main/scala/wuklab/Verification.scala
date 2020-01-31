package wuklab

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.{Axi4, Axi4Config, Axi4ReadOnly}
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Config, AxiLite4SlaveFactory}

// We first test this with a minimal config
class MemoryRegisterInterface(addressWidth : Int) extends Component with XilinxAXI4Toplevel {

  assert(addressWidth <= 32)

  val axiliteConfig = AxiLite4Config(32, 32)
  val axi4Config = Axi4Config(
    addressWidth = addressWidth,
    dataWidth = 32,
    useId = false,
    useRegion = false,
    useBurst = true,
    useLock = false,
    useQos = false,
    useLen = true,
    useResp = false,
    useProt = false,
    useCache = false
  )

  val io = new Bundle {
    val regBus = slave (AxiLite4(axiliteConfig))
    val memBus = master (Axi4ReadOnly(axi4Config))
  }

  val counter = Reg(UInt(32 bits)) init 0
  counter := counter + 1

  val readEnable  = Reg(UInt(32 bits)) init 0
  val readAddress = Reg(UInt(32 bits))
  val readResult  = Reg(UInt(32 bits)) init 0xdead
  val startTime   = Reg(UInt(32 bits))
  val endTime     = Reg(UInt(32 bits))

  val regs = new AxiLite4SlaveFactory(io.regBus)
  regs.write        (readEnable, 0)
  regs.readAndWrite (readAddress, 4)
  regs.read         (readResult, 8)
  regs.read         (startTime, 12)
  regs.read         (endTime, 16)

  val readCtrl = new Area {
    // use a fifo to set ready to high
    // TODO: replace this with a registerFIFO
    val fifo = new StreamFifoLowLatency(io.memBus.readCmd.payload, 1, 0)
    fifo.io.pop >> io.memBus.readCmd

    val read = fifo.io.push.ready & readEnable(0)

    fifo.io.push.valid := read
    fifo.io.push.addr := readAddress(addressWidth-1 downto 0)
    fifo.io.push.size := Axi4.size.BYTE_8.asUInt
    // This is length code, burst size = len + 1
    fifo.io.push.len := 0
    fifo.io.push.setBurstINCR

    when(read) {
      startTime := counter
      readEnable.clearAll()
    }
  }

  val writeCtrl = new Area {
    io.memBus.readRsp.ready := True
    when (io.memBus.readRsp.valid) {
      endTime := counter
      readResult := io.memBus.readRsp.data(31 downto 0).asUInt
    }
  }

  addPrePopTask(renameIO)
}

