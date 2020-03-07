package wuklab

import spinal.core._

object GenerateContext {
  implicit val config : CoreMemConfig = new CoreMemConfig {
    val physicalAddrWidth = 40
    val virtualAddrWidth = 64
    val hashtableAddrWidth = 40
    val tagWidth = 40
    val ppaWidth = 16
    val pageSizes = Seq[Int](2,4,8)
    // Cache config
    val numCacheCells = 128
    val numPageFaultCacheCells = 16
    // Hash Table Config
    val hashtableBaseAddr = BigInt("B0000000", 16)
    val pteAddrWidth = 4

    val ptePerLine = 4
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
