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
    val fifo = new StreamFifo(from.payload, waterMark * 2)
    from.haltWhen(halt && fifo.io.availability < waterMark)
    from >> fifo.io.push

    fifo.io.pop
  }
}

object Utils {
  // Definations
  type Valid[T <: Data] = Flow[T]

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
      val next = Stream(flow.payloadType())
      next.valid := flow.valid
      next.payload := flow.payload
      next
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
    def mux[T <: Data](streams : Stream[T] *): Stream[T] = {
      assert(streams.size == 2)
      StreamMux(b.asUInt, streams)
    }
    def demux[T <: Data](stream : Stream[T]): Seq[Stream[T]] = {
      StreamDemux(stream, b.asUInt, 2)
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

trait NonStopablePipeline extends Pipeline {

  def delay [T <: Data](t : T) : T = {
    Delay(t, pipelineDelay)
  }
}

trait StoppablePipeline extends Pipeline {
  def enableSignal : Bool

  def delay [T <: Data](t : T) : T = {
    Delay(t, pipelineDelay, enableSignal)
  }
}
