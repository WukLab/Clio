package wuklab.kv

import wuklab.{kv, _}
import spinal.core._
import spinal.lib._
import Utils._
import spinal.core.internals.Operator
import spinal.lib


class KeyValueEndPointWithPhysicalMemory extends Component {
  // Convert the DMA interface to the Physcial interfaece

}

class KeyValueEngine(implicit config : KeyValueConfig) extends Component {
  // One -> multiple
  // Queue for multiple instances
  //

  // Fetch the first bucket
  // Fetch the second bucket
  val io = {
    val ep = LegoMemEndPoint()
    val dma = AxiStreamDMAInterface()
  }

  // connect the modules

  // do arbitration on modules

}
