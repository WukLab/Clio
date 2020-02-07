package wuklab

import spinal.core._
import spinal.lib._

trait XilinxAXI4Toplevel {

  this : Component =>

  def renameIO() : Unit = {
    this.noIoPrefix()
    for (wire <- this.getAllIo) {
      val newName = wire.getName().replaceAll("(a?[wrb])_(payload_)?", "$1")
      println(wire, wire.getName(), newName)
      wire.setName(newName)
    }
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
object WaterMarkFifo {
  def apply[T <: Data](from : Stream[T], waterMark: Int, halt : Bool = False) : Stream[T] = {
    val fifo = new StreamFifo(from.payloadType, waterMark * 2)
    fifo.io.push << from.haltWhen(halt && fifo.io.availability < waterMark)
    fifo.io.pop
  }
}

trait WithValid[T <: Data] {
  def getValid : Bool
}

object Utils {
  // Definations
  type Valid[T <: Data] = Flow[T]

  implicit class IntUtils(i : Int) {
    def countBy (n : Int): Range = {
      (i + n - 1) downto i
    }

    def downBy (n : Int): Range = {
      i - 1 downto (i - n)
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
    def continueWhenPipeline(valid : Bool, delay : Int): Stream[T] = {
      // TODO: change queue size
      val buffer = stream.queue(delay)

      buffer.continueWhen(valid)
    }

    def fmap[T2 <: Data](f : T => T2) : Stream[T2] = {
      stream ~~ f
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
  }

  implicit class BundleUtils[T <: Bundle](bundle: T) {

    def withValid (valid : Bool) : Valid[T] = {
      val flow = Flow(bundle)
      flow.valid := valid
      flow.payload := bundle
      flow
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
        f.valid := idx === b.asUInt
        f
      })
    }
  }

  implicit class RichPipes[Y](y: Y) {
    def |>[Z](f: Y => Z) = f(y)
    def &>[X, Z](f: (X, Y) => Z): (X => Z) = (x: X) => f(x, y)
  }

  // Ungrouped functions
  // Fill 1s from right-most 1 to left
  def leftOR(x: UInt, width: Int): UInt = {
    val res = (1 until width) map (x |<< _) reduce (_ | _)
    res(width-1 downto 0)
  }
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

  // These methods DO change the internal value, so do not chain these methods
  def haltWhen(cond : Bool) : Unit = enableSignal := !cond
  def continueWhen(cond : Bool) : Unit = enableSignal := cond

  // TODO: find a way to initialize this delayed register
  def delay [T <: Data](t : T) : T = {
    import Utils._
    val init = cloneOf(t).clearAll
    Delay(t, pipelineDelay, enableSignal, init)
  }

  def delayFlow[T <: Data](t : T, valid : Bool) : Flow[T] = {
    val flow = Flow(t)
    flow.valid := delay(valid)
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

  // Since this is not a stream, we only have this direction
  // Without driving stream's ready
  def <|| [T <: Data](stream : Stream[T]) : Stream[T] = {
    delayStream(stream.payload, stream.valid)
  }
}
