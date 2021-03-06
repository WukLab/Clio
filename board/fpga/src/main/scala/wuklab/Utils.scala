package wuklab

import spinal.core._
import spinal.lib.{Stream, _}
import wuklab.Utils.AxiStream

// TODO: set composition name

trait XilinxAXI4Toplevel {

  this : Component =>

  def renameIO() : Unit = {
    this.noIoPrefix()
    for (wire <- this.getAllIo) {
      // For Axi memory mapped
      val newName = wire.getName()
        .replaceAll("(a?[wrb])_(payload_)?", "$1")
        // For Axi Stream, raw interface
        .replaceAll("_payload$", "_tdata")
        .replaceAll("_ready$", "_tready")
        .replaceAll("_valid$", "_tvalid")
        // For Axi Stream, Fragment Interface
        .replaceAll("_last$", "_tlast")
        .replaceAll("_payload_t", "_t")
        .replaceAll("_payload_fragment_t", "_t")
        .replaceAll("_payload_", "_")
        // Special rules
        .replaceAll("_desc_tready$", "_desc_ready")
        .replaceAll("_desc_tvalid$", "_desc_valid")
        // fragments
        .replaceAll("_fragment$", "_tdata")
      wire.setName(newName)
    }
    println(f"Xilinx: Rename ${this.getClass().getSimpleName}")
  }

  // TODO: find a solution to this
  // addPrePopTask(renameIO)

}

object StreamJoinMaster{
  def arg(sources : Stream[_]*) : Event = apply(sources.seq)
  def apply(sources : Seq[Stream[_]]) : Event = {
    assert(sources forall(_.isMasterInterface))

    val event = Event
    val eventFire = event.fire
    event.ready := sources.map(_.ready).reduce(_ && _)
    sources.foreach(_.valid := eventFire)
    event
  }
  def apply[T <: Data](sources : Seq[Stream[_]],payload : T) : Stream[T] = StreamJoin(sources).translateWith(payload)
}

// The first way to solve a pipeline problem, a almostfull fifo
// in      -> |     | -> res.fst
// res.snd <- |     | <- out
object WaterMarkFifo {
  def apply[T <: Data, T2 <: Data](in : Stream[T], out : Stream[T2], waterMark: Int, halt : Bool = False)
    : (Stream[T], Stream[T2]) = {
    val fifo = new StreamFifo(out.payloadType, waterMark * 2)
    out >> fifo.io.push
    (in.haltWhen(halt || fifo.io.availability < waterMark), fifo.io.pop)
  }
}

trait WithValid[T <: Data] {
  def getValid : Bool
}

object Utils {
  // Definations
  type Valid[T <: Data] = Flow[T]
  // We only need an extra last for this type
  type AxiStream[T <: Data] = Stream[Fragment[T]]

  // Helper Functions
  def RegNextWhenBypass[T <: Data](input : T, when : Bool) : T = {
    val reg = RegNextWhen(input, when)
    Mux(when, input, reg)
  }

  //
  implicit class UIntUtils(u : UInt) {
    def maskBy(m : Int) : UInt = {
      val mask = m.toMask.asUInt
      u(mask.getWidth-1 downto 0) & mask
    }
  }
  implicit class IntUtils(i : Int) {
    def countBy (n : Int): Range = {
      (i + n - 1) downto i
    }

    def downBy (n : Int): Range = {
      i - 1 downto (i - n)
    }

    def toMask: Bits = {
      Bits(log2Up(i) bits).setAll()
    }
  }

  implicit class DataUtils[T <: Data](t : T) {
    def @= (that : T) = {
      t := that
      t
    }

    def clearAll = {
      t.flatten.filter(_ != null).foreach {
        case u : UInt => u := 0
        case b : Bits => b := 0
        case b : Bool => b := False
      }
      t
    }
  }

  // Another way is stopable pipeline
  implicit class StreamUtils[T <: Data](stream : Stream[T]) {
    def fmap[T2 <: Data](f : T => T2) : Stream[T2] = {
      stream ~~ f
    }
    def changeBy(f : T => Unit) : Stream[T] = {
      val next = cloneOf(stream)
      next << stream
      next.payload.allowOverride
      f(next.payload)
      next
    }

    def lift[T2 <: Data](implicit f : T => T2) : Stream[T2] = {
      stream ~~ f
    }

    def branch() : (Stream[T], Stream[T]) = {
      val next = Stream(stream.payload)
      next.payload := stream.payload
      next.valid := stream.valid
      (stream.continueWhen(next.ready), next.continueWhen(stream.ready))
    }

    // This are now all apply to the end.
    // This may be not an Applicative, but I love this symbol!
    def *> [T2 <: Data](r : Stream[T2]) : Stream[T2] = {
      StreamJoin.arg(stream, r).translateWith(r.payload)
    }
    def <* (r : Stream[_]) : Stream[T] = {
      StreamJoin.arg(stream, r).translateWith(stream.payload)
    }

    def >*< [T2 <: Data](r : Stream[T2]) : Stream[Pair[T, T2]] = {
      StreamJoin.arg(stream, r).translateWith(Pair(stream.payload, r.payload))
    }

    def joinWhen[T2 <: Data](r : Stream[T2], byPassWhen : Bool) : Stream[T2] = {
      val next = Stream(r.payloadType)
//      stream.ready := Mux(byPassWhen, next.ready, next.ready && r.valid)
//      r.ready := !byPassWhen && next.ready && stream.valid
//      afterAlloc.valid := Mux(noAlloc, lookupResult.valid, lookupResult.valid && selectedAddr.valid)
//      afterAlloc.payload.fst := noAlloc
//      afterAlloc.payload.snd := lookupResult.payload
//      when (!lookupResult.used) {
//        afterAlloc.snd.used := True
//        afterAlloc.snd.ppa := selectedAddr.payload
//      }
      next
    }

    // tap means tap the fire event
    def tapAsFlow : Flow[T] = {
      val next = Flow(stream.payloadType())
      next.valid := stream.fire
      next.payload := stream.payload
      next
    }

    def tap: Stream[T] = {
      val next = Stream(stream.payloadType())
      next.valid := stream.fire
      next.payload := stream.payload
      next
    }

    def continueBy(f : T => Bool) : Stream[T] = {
      stream.continueWhen(f(stream.payload))
    }
    def throwBy(f : T => Bool) : Stream[T] = {
      stream.throwWhen(f(stream.payload))
    }
    def takeBy(f : T => Bool) : Stream[T] = {
      stream.takeWhen(f(stream.payload))
    }

    def disable : Unit = {
      stream.payload.clearAll
      stream.valid.clear()
    }

    def mergeBy[T2 <: Data](f : T => Bool, other : Stream[T2])(assign : (T, T2) => Unit) : Stream[T] = {
      val next = cloneOf(stream)
      val cond = f(stream.payload)
      next.payload := stream.payload
      when (cond) { assign(next.payload, other.payload) }

      stream.ready := Mux(cond, other.valid && next.ready, next.ready)
      other.ready := cond && stream.valid && next.ready
      next.valid := Mux(cond, other.valid && stream.valid, stream.valid)

      next
    }


    def forkWith(signal : Stream[Bool]) : (Stream[T], Stream[T]) = {
      val (ifTrue, ifFalse) = StreamFork2(stream)

      ifTrue.continueWhen(signal.valid && signal.payload)
      ifFalse.continueWhen(signal.valid && !signal.payload)

      signal.ready := ifTrue.fire || ifFalse.fire
      (ifTrue, ifFalse)
    }

    def addLatency(n : Int) : Stream[T] = {
      (0 until n).foldLeft(stream) { (s, _) => s.stage() }
    }

  }

  implicit class StreamPairUtils[T1 <: Data, T2 <: Data](stream : Stream[Pair[T1, T2]]) {
    // No comb path on valid and ready
    def traverse : (Stream[T1], Stream[T2]) = {
      val fst = Stream(stream.fst)
      val snd = Stream(stream.snd)
      stream.ready := fst.ready & snd.ready
      fst.valid := stream.fire
      snd.valid := stream.fire
      (fst.continueWhen(snd.ready), snd.continueWhen(fst.ready))
    }

    def fmapFst [T <: Data](f : T1 => T): Stream[Pair[T, T2]] = {
      stream.fmap(p => Pair(f(p.fst), p.snd))
    }
    def fmapSnd [T <: Data](f : T2 => T): Stream[Pair[T1, T]] = {
      stream.fmap(p => Pair(p.fst, f(p.snd)))
    }

    def bimap [T <: Data](f : (T1, T2) => T) : Stream[T] = {
      stream.fmap(p => f(p.fst, p.snd))
    }
  }

  implicit class FlowUtils[T <: Data](flow : Flow[T]) {
    def bind[T2 <: Data](f : T => Flow[T2]) : Flow[T2] = {
      val trans = f(flow.payload)
      val next = Flow(trans.payload)
      next.valid := trans.valid & flow.valid
      next.payload := trans.payload
      next
    }

    def fmap[T2 <: Data](f : T => T2) : Flow[T2] = {
      val payload = f(flow.payload)
      val next = Flow(payload)
      next.valid := flow.valid
      next.payload := payload
      next
    }

    def asStream : Stream[T] = {
      val next = Stream(flow.payload)
      next.valid := flow.valid
      next.payload := flow.payload
      next
    }

    def takeBy(f : T => Bool) : Flow[T] = {
      flow.takeWhen(f(flow.payload))
    }

    def throwBy(f : T => Bool) : Flow[T] = {
      flow.throwWhen(f(flow.payload))
    }
  }

  implicit class BundleUtils[T <: Bundle](bundle: T) {

    def withValid (valid : Bool) : Valid[T] = {
      val flow = Flow(bundle)
      flow.valid := valid
      flow.payload := bundle
      flow
    }

    def <~ (f : T => Unit) : T = {
      val next = cloneOf(bundle)
      next := bundle
      next.allowOverride
      f(next)
      next
    }

  }

  implicit class BitsUtils(bits : Bits) {
    def fieldChange(offset : Int, width : Int)(f : Bits => Bits) : Bits = {
      val next = cloneOf(bits)
      next.allowOverride
      next := bits
      next(offset, width bits) := f(next(offset, width bits))
      next
    }
  }

  implicit class BoolUtils(b : Bool) {
    def select[T <: Data](streams : Stream[T] *): Stream[T] = {
      assert(streams.size == 2)
      StreamMux(b.asUInt, streams)
    }
    def demux[T <: Data](stream : Stream[T]): Seq[Stream[T]] = {
      StreamDemux(stream, b.asUInt, 2)
    }
    def demux[T <: Data](flow : Flow[T]): Seq[Flow[T]] = {
      (0 until 2).map(idx => {
        val f = Flow(flow.payloadType)
        f.payload := flow.payload
        f.valid := (idx === b.asUInt) && flow.valid
        f
      })
    }
  }

  implicit class StreamFragmentUtils[T <: Data](frag : Stream[Fragment[T]]) {

    def changeFirst (f : T => Unit) : Stream[Fragment[T]] = {
      val next = cloneOf(frag)
      next.last := frag.last
      next.valid := next.valid
      next.fragment := frag.fragment
      when (frag.first) { f(next.fragment) }
      next
    }

    def fmapFirst (f : T => T) : Stream[Fragment[T]] = {
      val next = cloneOf(frag)
      next.last := frag.last
      next.valid := next.valid
      next.fragment := Mux(frag.first, f(frag.fragment), frag.fragment)
      next
    }

    // Filter message by signal
    def filterBySignal(signal : Stream[Bool]): Stream[Fragment[T]] = {
      val next = cloneOf(frag)

      next.payload := frag.payload
      next.valid := signal.payload && signal.valid && frag.valid
      frag.ready := Mux(signal.payload, signal.valid && next.ready, signal.valid)
      signal.ready := frag.lastFire
      next
    }

    def syncWith[T2 <: Data](stream : Stream[T2]): (Stream[Fragment[T]], Stream[T2]) = {
      val nextStream = cloneOf(stream)
      val nextFrag = cloneOf(frag)

      nextStream << stream.continueWhen (frag.last && frag.valid && nextFrag.ready)
      nextFrag   << frag.continueWhen   (stream.valid && Mux(frag.last, nextStream.ready, True))

      (nextFrag, nextStream)
    }

    def forkBySignal(signal : Stream[Bool]) : (Stream[Fragment[T]], Stream[Fragment[T]]) = {
      val ifTrue = cloneOf(frag)
      val ifFalse = cloneOf(frag)

      ifTrue.payload := frag.payload
      ifFalse.payload := frag.payload

      ifTrue.valid := signal.payload && signal.valid && frag.valid
      ifFalse.valid := !signal.payload && signal.valid && frag.valid

      frag.ready := signal.valid && Mux(signal.payload, ifTrue.ready, ifFalse.ready)
      signal.ready := frag.lastFire

      (ifTrue, ifFalse)
    }

    def translateWithLastInsert(data : T, requireInsert : Bool): Stream[Fragment[T]] = {
      // 1-bit state machine
      val waitInsert = Reg (Bool) init True
      val ret = cloneOf (frag)
      val doInsert = frag.last && waitInsert && requireInsert

      ret.fragment := data
      ret.last := frag.last && !doInsert
      ret.valid := frag.valid
      frag.ready := ret.ready && !doInsert

      when (frag.lastFire) { waitInsert := !waitInsert }
      ret
    }

    def liftStream[T2 <: Data](f : T => T2) : Stream[Fragment[T2]] = {
      val ret = Stream Fragment f(frag.fragment)
      ret.translateFrom (frag) { (f1, f2) =>
        f1.fragment := f(f2.fragment)
        f1.last := f2.last
      }
    }

    def demux(select : Stream[UInt], num: Int) : Seq[Stream[Fragment[T]]] = {
      val fragments = StreamDemux(frag.continueWhen(select.valid), select.payload, num)
      select.ready := frag.lastFire
      fragments
    }

    def takeFirst : Stream[Fragment[T]] = frag.takeWhen(frag.first)


  }

  implicit class BitStreamFragmentUtils(frag : Stream[Fragment[Bits]]) {
    // 0 -----------> 512
    // initial | (head | tail) | (head | tail) | (head | tail)
    // (initial | head) | (tail | head) | ...
    def dropAt(offset : Int) : Stream[Fragment[Bits]] = {
      val tailWidth = widthOf(frag.fragment)
      val next = cloneOf(frag)
      // TODO: finish this

      next
    }

    def shiftAt(initial : Bits, skipLast : Bool): Stream[Fragment[Bits]] = {
      // TODO: we may waste a cycle here, repair this!
      val next = cloneOf(frag)
      val offset = frag.fragment.getWidth - initial.getWidth

      val tailReg = RegNextWhen(frag.fragment(frag.fragment.getWidth - 1 downto offset), frag.fire)
      val shiftedData = frag.fragment(offset - 1 downto 0) ## Mux(frag.first, initial, tailReg)

      val longIn = frag.insertHeader(shiftedData)
      val in = longIn.throwWhen(longIn.first && skipLast)

      next.translateFrom (in) { (dst, src) =>
        dst.fragment := shiftedData
        dst.last := src.last
      }

      next
    }
  }

  // General classes
  implicit class RichPipes[Y](y: Y) {
    def |>[Z](f: Y => Z) = f(y)
    def &>[X, Z](f: (X, Y) => Z): (X => Z) = (x: X) => f(x, y)
  }

  implicit class ExtendedRandom(ran: scala.util.Random) {
    def nextByteArray(size: Int) = {
      val arr = new Array[Byte](size)
      ran.nextBytes(arr)
      arr
    }
  }

  // Ungrouped functions
  // Fill 1s from right-most 1 to left
  def leftOR(x: UInt, width: Int): UInt = {
    val res = (1 until width) map (x |<< _) reduce (_ | _)
    res(width-1 downto 0)
  }

  def rightOR(x : UInt) : UInt = (1 until x.getWidth) map (x |>> _) reduce (_ | _)
  def unique(bits : Bits) : Bits = bits & (~bits.asUInt + 1).asBits
}

object Pair {
  def apply[T1 <: Data, T2 <: Data](p : (T1, T2)): Pair[T1,T2] = new Pair(p._1, p._2)
}

case class Pair[T1 <: Data, T2 <: Data](fstValue: T1, sndValue: T2) extends Bundle {
  val fst = cloneOf(fstValue)
  val snd = cloneOf(sndValue)
  fst := fstValue
  snd := sndValue
}

object ReturnStream {
  def apply[T <: Data](payload : T, valid : Bool = True) : Stream[T] = {
    val s = new Stream(payload)
    s.payload := payload
    s.valid := valid
    s
  }
}

object ReturnFlow {
  def apply[T <: Data](payload : T, valid : Bool = True) : Flow[T] = {
    val s = new Flow(payload)
    s.payload := payload
    s.valid := valid
    s
  }
}

// MISC
object FlowMux {
  def apply[T <: Data](sel : UInt)(flows : Flow[T]*) : Flow[T] = {
    val next = cloneOf (flows(0))
    assert(flows.size <= sel.maxValue)
    next.valid := flows(sel).valid
    next.payload := flows(sel).payload
    next
  }
}

object StreamDemux2 {
  def apply[T <: Data](stream : Stream[T], sel : Bool) = StreamDemux(stream, sel.asUInt, 2)
}
object StreamDemuxBy2 {
  def apply[T <: Data](stream : Stream[T])(sel : T => Bool) = StreamDemux(stream, sel(stream.payload).asUInt, 2)
}

object StreamReorder {
  def apply[T <: Data](l : Stream[T], r : Stream[T])(order : T => UInt) : Stream[T] = {
    val sel = order(l.payload) < order(r.payload)
    StreamMux(sel.asUInt, Seq(r, l))
  }
}


// traits
trait Pipeline {
  def pipelineDelay : Int

  def delay [T <: Data](t : T) : T
}

trait NonStoppablePipeline extends Pipeline {

  def delay [T <: Data](t : T) : T = {
    import Utils._
    val init = cloneOf(t).clearAll
    Delay(t, pipelineDelay, init = init)
  }
}

trait StoppablePipeline extends Pipeline {
  def enableSignal : Bool

  // TODO: find a way to initialize this delayed register
  def delay [T <: Data](t : T) : T = {
    import Utils._
    val init = cloneOf(t).clearAll
    Delay(t, pipelineDelay, enableSignal, init)
  }

  // TODO: check the valid signal.
  def delayFlow[T <: Data](t : T, valid : Bool) : Flow[T] = {
    val flow = Flow(t)
    flow.valid := delay(valid) & enableSignal
    flow.payload := delay(t)
    flow
  }

  // We should make sure the ready is enable signal
  def delayStream[T <: Data](t : T, valid : Bool) : Stream[T] = {
    val stream = Stream(t)
    stream.valid := delay(valid)
    stream.payload := delay(t)
    stream
  }

  def streamDelay[T <: Data](stream : Stream[T]) = stream.queueLowLatency(log2Up(pipelineDelay))

  // Inject Operator
  // Since this is not a stream, we only have this direction
  // Without driving stream's ready
  def <|| [T <: Data](stream : Stream[T]) : Stream[T] = {
    delayStream(stream.payload, stream.valid)
  }
}

// TODO: better impl
object StreamDemuxOH {
  def apply[T <: Data](stream : Stream[T], selectOH: Vec[Bool]) = {
    val numPorts = selectOH.size
    StreamDemux(stream, OHToUInt(selectOH), numPorts)
  }

}

object StreamWithFragmentArbiter {
  import Utils._

  def apply[T1 <: Data, T2 <: Data]
  (streams : Seq[Stream[T1]], fragments : Seq[Stream[Fragment[T2]]]) : (Stream[T1], Stream[Fragment[T2]])= {

    require(streams.length > 0, "Should work on a list")
    require(streams.length == fragments.length, "Number of the streams should match fragments")

    val numPairs = streams.length
    val fragType = fragments(0).payloadType

    val dataArbiter = StreamArbiterFactory.lowerFirst.fragmentLock.build(fragType, numPairs)
    (dataArbiter.io.inputs, fragments).zipped map { _ << _ }

    val (dataOutputStream, dataRouteStreamF) = StreamFork2(dataArbiter.io.output)

    val dataRouteStream = dataRouteStreamF.takeFirst.translateWith(dataArbiter.io.chosen).queueLowLatency(4)
    val cmdOutputStream = StreamMux(dataRouteStream.payload, streams) <* dataRouteStream

    (cmdOutputStream, dataOutputStream)
  }

  def onInterface[T <: Data, T1 <: Data, T2 <: Data](ts : T *)
  (select : T => (Stream[T1], Stream[Fragment[T2]])) : (Stream[T1], Stream[Fragment[T2]]) = {
    val (cs, ds) = ts.map(select).unzip
    apply(cs, ds)
  }
}

object StreamWithFragmentDemux {
  import Utils._

  def apply[T1 <: Data, T2 <: Data]
  (num : Int, select : Stream[UInt])(stream : Stream[T1], fragment : Stream[Fragment[T2]]) : (Seq[Stream[T1]], Seq[Stream[Fragment[T2]]]) = {
    val (streamSel, fragSel) = StreamFork2(select)
    val streams = StreamDemux(stream <* streamSel, streamSel.payload, num)
    val fragments = StreamDemux(fragment.continueWhen(fragSel.valid), fragSel.payload, num)
    fragSel.throwWhen(fragment.fire)
    (streams, fragments)
  }

  def onInterface[T <: Data, T1 <: Data, T2 <: Data]
  (num : Int, selectFunc : T1 => UInt)(t : T, select : T => (Stream[T1], Stream[Fragment[T2]]))
  : (Seq[Stream[T1]], Seq[Stream[Fragment[T2]]]) = {
    val (streamF, fragment) = select(t)
    val (stream, selectStreamF) = StreamFork2(streamF)
    val selectStream = selectStreamF.fmap(selectFunc)
    apply(num, selectStream)(stream, fragment)
  }


}
