package wuklab

import spinal.core._
import spinal.lib._

// This module converts a streaming interface to LegoMem Requests
// Also this one will provide
// class LegoMemDMA(config : CoreMemConfig) extends Component {
//   val io = new Bundle {
//     val dma = new Bundle {
//       val writeCmd = slave Stream AxiStreamDMAWriteCommand()
//       val readCmd  = slave Stream AxiStreamDMAReadCommand()
//       val writeData = slave Stream Fragment(Bits(512 bits))
//       val readData = master Stream Fragment(Bits(512 bits))
//     }
//
//     val lego = new Bundle {
//       val dataIn = slave Stream Fragment(Bits(512 bits))
//       val dataOut = master Stream Fragment(Bits(512 bits))
//     }
//   }
//
//   val accessHeader = LegoMemAccessHeader(config.virtualAddrWidth)
//
//   val readHeader = Stream(accessHeader).translateFrom (io.dma.readCmd) { (header, cmd) =>
//     header.header.reqType := LegoMem.RequestType.READ
//     header.header.size    := header.packedBytes
//     header.header.cont    := LegoMem.Continuation(LegoMem.Continuation.EP_COREMEM, LegoMem.Continuation.EP_KEYVALUE)
//     header.length := cmd.len
//     header.addr   := cmd.addr
//   }
//   val writeHeader = Stream(accessHeader).translateFrom (io.dma.writeCmd) { (header, cmd) =>
//     header.header.reqType := LegoMem.RequestType.WRITE
//     header.header.size    := header.packedBytes + cmd.len
//     header.header.cont    := LegoMem.Continuation(LegoMem.Continuation.EP_COREMEM, LegoMem.Continuation.EP_KEYVALUE)
//     header.length := cmd.len
//     header.addr   := cmd.addr
//   }
//
//   // Since the request is inorder, we do not have to reorder the info
//   val packetBuilder = new PacketBuilder(512, accessHeader)
//   packetBuilder.io.headerIn << StreamArbiterFactory.lowerFirst.onArgs(readHeader, writeHeader)
//   packetBuilder.io.dataIn << io.dma.readData
//   packetBuilder.io.dataOut >> io.lego.dataOut
//
//   val packetParser = new PacketParser(512, LegoMemHeader())
//   packetParser.io.headerOut.freeRun()
//   packetParser.io.dataOut >> io.dma.readData
//   packetParser.io.dataIn << io.lego.dataIn
//
// }

