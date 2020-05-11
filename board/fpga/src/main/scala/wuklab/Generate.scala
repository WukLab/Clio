package wuklab

import spinal.core._

object GenerateContext {
  val ddrBaseAddr = BigInt("500000000", 16)
  implicit val config : CoreMemConfig = new CoreMemConfig {
    val physicalAddrWidth = 40
    val virtualAddrWidth = 64

    // Virtual Memory Config
    val pageSizes = Seq(4 MiB, 16 MiB, 64 MiB).map(_.toInt)
    val fullTagWidth = 80
    val tagOffset = 20

    // Cache config
    val numCacheCells = 64
    val numPageFaultCacheCells = 16

    // Hash Table Config
    val hashtableBaseAddr = ddrBaseAddr + (512 MiB)
    val numBuckets = 512
    val ptePerBucket = 16
  }
}

//Define a custom SpinalHDL configuration with synchronous reset instead of the default asynchronous one. This configuration can be resued everywhere
object MySpinalConfig extends SpinalConfig(
  defaultConfigForClockDomains = ClockDomainConfig(
    resetKind = SYNC,
    resetActiveLevel = LOW
  )
)

//Generate the MyTopLevel's Verilog using the above custom configuration.
object MyTopLevelVerilogWithCustomConfig {
  def main(args: Array[String]) {
    MySpinalConfig.generateVerilog(new MemoryRegisterInterface(32))
  }
}

object AddressLookupVerification {
  def main(args: Array[String]) {
    import GenerateContext._
    MySpinalConfig.generateVerilog(new CoreLookupWrapper)
  }
}

object LegoMemSystemGenerate {
  def main(args: Array[String]) {
    import GenerateContext._
    val report = MySpinalConfig.generateVerilog(new LegoMemSystem)
    report.mergeRTLSource("LegoMemSystemLib")
  }
}

object PingPongGenerate {
  def main(args: Array[String]) {
    import GenerateContext._
    MySpinalConfig.generateVerilog(new PingPong(8))
  }
}

object MonitorRegistersGenerator {
  def main(args: Array[String]) {
    import GenerateContext._
    MySpinalConfig.generateVerilog(new monitor.MonitorRegisters(0, 3, BigInt("A000C000", 16)))
  }
}
