package wuklab

// System Top Level Class

import spinal.core._
import spinal.lib._
import spinal.lib.bus.amba4.axi.Axi4
import Utils._
import spinal.core
import spinal.lib.bus.amba4.axilite.{AxiLite4, AxiLite4Config, AxiLite4SlaveFactory}

class LegoMemSystem(implicit config : CoreMemConfig) extends Component with XilinxAXI4Toplevel {
  val debug = true
  val socAxisConfig = AxiStreamConfig(512, keepWidth = 64)
  val socCtrlAxisConfig = AxiStreamConfig(64, keepWidth = 8)

  val io = new Bundle {
    // End Points to Cross bar
    val eps = new Bundle {
      val seq = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
      val mem = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
      val soc = LegoMemEndPoint(config.epDataAxisConfig, config.epCtrlAxisConfig)
    }

    // Other external interface
    val net = NetworkInterface(config.networkDataWidth)
    val net_sess = master Stream UInt(16 bits)
    val soc = new Bundle {
      val dataIn  = master Stream Fragment (AxiStreamPayload(socAxisConfig))
      val dataOut = slave  Stream Fragment (AxiStreamPayload(socAxisConfig))
      val ctrlIn  = master Stream Fragment (AxiStreamPayload(socCtrlAxisConfig))
      val ctrlOut = slave  Stream Fragment (AxiStreamPayload(socCtrlAxisConfig))
    }
    val bus = new Bundle {
      val access = master (Axi4(config.accessAxi4Config))
      val lookup = master (Axi4(config.lookupAxi4Config))
    }

    // Debug Interfaces
    val regBus = slave (AxiLite4(AxiLite4Config(32, 32)))
  }

  val sequencer = new MemoryModel
  sequencer.io.ep <> io.eps.seq
  sequencer.io.net <> io.net
  sequencer.io.sess >> io.net_sess

  val access = new MemoryAccessEndPoint
  access.io.ep <> io.eps.mem
  access.io.bus <> io.bus

  // Soc acceses
  val soc = new RawInterfaceEndpoint
  soc.io.ep <> io.eps.soc
  // TODO: do not handle multiple cycle control messages
  io.soc.ctrlIn.translateFrom (soc.io.raw.ctrlIn) { (frag, ctrl) =>
    frag.fragment.tdata := ctrl.asBits
    frag.fragment.tkeep := B"8'hFF"
    frag.last := True
  }
  soc.io.raw.ctrlOut.translateFrom (io.soc.ctrlOut) { (ctrl, frag) => ctrl.assignFromBits(frag.fragment.tdata)}

  val bridge = new LegoMemAxiStreamBridge(socAxisConfig)
  bridge.io.lego.dataIn << soc.io.raw.dataIn
  bridge.io.lego.dataOut >> soc.io.raw.dataOut
  bridge.io.axis.dataIn << io.soc.dataOut
  bridge.io.axis.dataOut >> io.soc.dataIn

  // Debug Interface on EP
  val counter  = Counter(32 bits, True)

  val last = new Area {
    val epSocIn  = RegNextWhen(counter.value, io.eps.soc.dataIn .lastFire) init 0xdead
    val epSocOut = RegNextWhen(counter.value, io.eps.soc.dataOut.lastFire) init 0xdead
    val epMemIn  = RegNextWhen(counter.value, io.eps.mem.dataIn .lastFire) init 0xdead
    val epMemOut = RegNextWhen(counter.value, io.eps.mem.dataOut.lastFire) init 0xdead
    val epSeqIn  = RegNextWhen(counter.value, io.eps.seq.dataIn .lastFire) init 0xdead
    val epSeqOut = RegNextWhen(counter.value, io.eps.seq.dataOut.lastFire) init 0xdead
    val netIn    = RegNextWhen(counter.value, io.net.dataIn     .lastFire) init 0xdead
    val netOut   = RegNextWhen(counter.value, io.net.dataOut    .lastFire) init 0xdead
  }
  val first = new Area {
    val epSocIn  = RegNextWhen(counter.value, io.eps.soc.dataIn .firstFire) init 0xdead
    val epSocOut = RegNextWhen(counter.value, io.eps.soc.dataOut.firstFire) init 0xdead
    val epMemIn  = RegNextWhen(counter.value, io.eps.mem.dataIn .firstFire) init 0xdead
    val epMemOut = RegNextWhen(counter.value, io.eps.mem.dataOut.firstFire) init 0xdead
    val epSeqIn  = RegNextWhen(counter.value, io.eps.seq.dataIn .firstFire) init 0xdead
    val epSeqOut = RegNextWhen(counter.value, io.eps.seq.dataOut.firstFire) init 0xdead
    val netIn    = RegNextWhen(counter.value, io.net.dataIn     .firstFire) init 0xdead
    val netOut   = RegNextWhen(counter.value, io.net.dataOut    .firstFire) init 0xdead
  }

  val total = new Area {
    val mem = Reg (UInt(32 bits)) init 0
    val system = Reg (UInt(32 bits)) init 0
    val fullmem = Reg (UInt(32 bits)) init 0
    val fullsystem = Reg (UInt(32 bits)) init 0

    // last to last
    when (io.eps.mem.dataOut.lastFire) {
      mem := mem + (counter.value - last.epMemIn)
      fullmem := fullmem + (counter.value - first.epMemIn)
    }
    when (io.net.dataOut.lastFire) {
      system := system + (counter.value - last.netIn)
      fullsystem := fullsystem + (counter.value - first.netIn)
    }

  }

  val regs = new AxiLite4SlaveFactory(io.regBus)
  def offset(i : Int, base : Int = 0) = BigInt("A0006000", 16) + BigInt(i * 4) + BigInt(base)

  if (config.debug) {
    regs.read (U"32'h20200524",  offset(16 + 0)) // 0x44
    regs.read (last.epSocIn,  offset(16 + 1)) // 0x44
    regs.read (last.epSocOut, offset(16 + 2)) // 0x48
    regs.read (last.epMemIn,  offset(16 + 3)) // 0x4c
    regs.read (last.epMemOut, offset(16 + 4)) // 0x50
    regs.read (last.epSeqIn,  offset(16 + 5)) // 0x54
    regs.read (last.epSeqOut, offset(16 + 6)) // 0x58
    regs.read (last.netIn,    offset(16 + 7)) // 0x5c
    regs.read (last.netOut,   offset(16 + 8)) // 0x60

    // 0x80
    regs.read (first.epSocIn,  offset(32 + 1)) // 0x84
    regs.read (first.epSocOut, offset(32 + 2)) // 0x88
    regs.read (first.epMemIn,  offset(32 + 3)) // 0x8c
    regs.read (first.epMemOut, offset(32 + 4)) // 0x90
    regs.read (first.epSeqIn,  offset(32 + 5)) // 0x94
    regs.read (first.epSeqOut, offset(32 + 6)) // 0x98
    regs.read (first.netIn,    offset(32 + 7)) // 0x9c
    regs.read (first.netOut,   offset(32 + 8)) // 0xa0

    // 0x400
    access.io.counters.zipWithIndex.map { case (reg, idx) => regs.read(reg, offset(0x100 + idx)) }
    access.io.lookup_counters.zipWithIndex.map { case (reg, idx) => regs.read(reg, offset(0x200 + idx)) }
  }

  // Stat
  // 0x100
  regs.readAndWrite (total.mem, 0x100 + offset(0))
  regs.readAndWrite (total.system, 0x100 + offset(1))
  regs.readAndWrite (total.fullmem, 0x100 + offset(2))
  regs.readAndWrite (total.fullsystem, 0x100 + offset(3))

  // 0x200
  access.io.stat.latency.zipWithIndex.map { case (reg, idx) =>
    regs.read(reg, offset(idx, 0x200))
  }

  // Rename
  addPrePopTask(renameIO)
}

