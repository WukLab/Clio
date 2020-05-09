package wuklab

import spinal.lib._
import spinal.core._
import Utils._

// Lookup3 hash function, require dataWidth <= 96
class Lookup3(key : Int)(dataWidth : Int, hashWidth : Int) extends Component {

  require(dataWidth <= 96, "DataWidth should be with in 96 bits")
  require(hashWidth <= 32, "OutputWidth should be less than 32 bits")

  type LoopVar = (UInt, UInt, UInt)
  type Selector = LoopVar => UInt

  val io = new Bundle {
    val enable = in Bool
    val data = in UInt (dataWidth bits)
    val hash = out UInt (hashWidth bits)
  }

  val lastPair = initPair(io.data) |> finalMix
  io.hash := lastPair._3.resize(hashWidth)

  def initPair(data : UInt) : LoopVar = {
    // TODO: check this with CPU alignment
    val fullData = data.resize(96)
    val a = U"32'hdeadbeef" + (dataWidth / 32) + fullData(31 downto 0)
    val b = U"32'hdeadbeef" + (dataWidth / 32) + fullData(63 downto 32)
    val c = U"32'hdeadbeef" + (dataWidth / 32) + fullData(95 downto 64)
    (a, b, c)
  }

  def finalMix(pair : LoopVar) : LoopVar = {
    // ref impl
    // c ^= b; c -= rot(b,14);
    // a ^= c; a -= rot(c,11);
    // b ^= a; b -= rot(a,25);
    // c ^= b; c -= rot(b,16);
    // a ^= c; a -= rot(c,4);
    // b ^= a; b -= rot(a,14);
    // c ^= b; c -= rot(b,24);

    val a : Selector = _._1
    val b : Selector = _._2
    val c : Selector = _._3

    // Scala Magic!
    (pair
      |> finalR(c, b, 14)
      |> finalR(a, c, 11)
      |> regPair(io.enable)
      |> finalR(b ,a, 25)
      |> finalR(c, b, 16)
      |> regPair(io.enable)
      |> finalR(a, c, 4)
      |> finalR(b, a, 14)
      |> regPair(io.enable)
      |> finalR(c, b, 24)
      )
  }

  def nextPair (pair : LoopVar) : LoopVar = {
    val next = ( cloneOf(pair._1), cloneOf(pair._2), cloneOf(pair._3) )
    next._1 := pair._1
    next._2 := pair._2
    next._3 := pair._3
    next._1.allowOverride
    next._2.allowOverride
    next._3.allowOverride
    next
  }

  def regPair (enable : Bool)(pair : LoopVar) : LoopVar =
    ( RegNextWhen(pair._1, enable), RegNextWhen(pair._2, enable), RegNextWhen(pair._3, enable) )

  def finalR(s1 : Selector, s2 : Selector, rot : Int)(pair : LoopVar) : LoopVar = {
    val next = nextPair(pair)
    s1(next) := (s1(pair) ^ s2(pair)) - s2(pair).rotateRight(rot)
    next
  }
}
