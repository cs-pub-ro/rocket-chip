// See LICENSE.Berkeley for license details.
// See LICENSE.SiFive for license details.

package freechips.rocketchip.tile

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config.Parameters
// Import only the concrete hgl modules we instantiate, so that the tile-package
// NRS objects (NRS_POSIT, NRS_MORRIS, ...) are not shadowed by the hgl `NRS`
// trait / objects defined in the same library.
import ro.upb.nrs.hgl.{
  FloatingPoint, DecoderFloatingPoint, EncoderFloatingPoint,
  FloatingPointComparator, FloatingPointToInteger, IntegerToFloatingPoint
}

/**
 * OP field (funct7[28:25], 4 bits) for the custom NRS instruction.
 */
object NRSOp {
  val ADD          = 0x0.U(4.W)
  val SUB          = 0x1.U(4.W)
  val MUL          = 0x2.U(4.W)
  val DIV          = 0x3.U(4.W)
  val SQRT         = 0x4.U(4.W)
  val FEQ          = 0x5.U(4.W)
  val FLT          = 0x6.U(4.W)
  val FLE          = 0x7.U(4.W)
  val FMIN         = 0x8.U(4.W)
  val FMAX         = 0x9.U(4.W)
  val SGNJ         = 0xA.U(4.W)
  val SGNJN        = 0xB.U(4.W)
  val SGNJX        = 0xC.U(4.W)
  val CVT_TO_INT   = 0xD.U(4.W)
  val CVT_FROM_INT = 0xE.U(4.W)
}

/**
 * Generic NRS arithmetic unit (GFPU-style) for the custom instruction.
 *
 * Custom opcode 0101011, R-type:
 *   [31:25] funct7  – OP is the low 4 bits [28:25]
 *   [14:12] funct3  – NRS_ID (selects one of 6 NRS types at runtime)
 *
 * Unlike a bank of per-NRS units, this is a single generic FPU: both operands
 * are decoded through *every* NRS into a *shared* internal FloatingPoint form
 * (width = max internal exp/frac across the supported NRS), the requested
 * operation runs on one arithmetic core, and the result is re-encoded through
 * every NRS. NRS_ID selects which decode/encode applies (operand1Type =
 * operand2Type = resultType = NRS_ID — single format per instruction; the
 * structure leaves room for independent per-operand types later).
 *
 *   000 Morris(4,32,RZ)   001 MorrisHEB(4,32,RZ)   010 MorrisBiasHEB(4,32,RE)
 *   011 Posit(32,2,RE)    100 Posit(32,3,RE)       101 MorrisUnaryHEB(32,RE)
 *
 * Operands are 32-bit NRS encodings in the low bits of FP registers; the raw
 * result is NaN-boxed in the FPU writeback path.
 */
class NRSUnit(val latency: Int)(implicit p: Parameters) extends FPUModule()(p) {
  val io = IO(new Bundle {
    val in    = Flipped(Valid(new FPInput))
    val out   = Valid(new FPResult)
    val nrsId = Input(UInt(3.W))  // inst[14:12]
    val op    = Input(UInt(4.W))  // inst[28:25]
  })

  private val t          = FType.S
  private val expWidth   = t.exp                 // 8
  private val sigWidth   = t.sig                 // 24
  private val totalWidth = expWidth + sigWidth   // 32

  // Supported NRS types, indexed by NRS_ID.
  private val nrss: Seq[NRS] = Seq(
    NRS_MORRIS,           // 000
    NRS_MORRIS_HEB,       // 001
    NRS_MORRIS_BIAS_HEB,  // 010
    NRS_POSIT,            // 011  Posit es2
    NRS_POSIT_ES3,        // 100  Posit es3
    NRS_MORRIS_UNARY_HEB  // 101
  )

  // ── Shared internal FloatingPoint width (max across all NRS) ────────────────
  private val ie    = nrss.map(_.getInternalExponentSize(expWidth, sigWidth)).max
  private val ifrac = nrss.map(_.getInternalFractionSize(expWidth, sigWidth)).max

  private def setRound(enc: EncoderFloatingPoint): Unit =
    enc.io.roundingType match { case Some(r) => r := 0.U /* RoundEven */; case None => }

  private def selById[T <: Data](head: T, cases: Seq[T]): T =
    MuxCase(head, cases.zipWithIndex.map { case (c, i) => (io.nrsId === i.U) -> c })

  val in1 = io.in.bits.in1(totalWidth - 1, 0)
  val in2 = io.in.bits.in2(totalWidth - 1, 0)

  // ── Decode each operand through every NRS into the shared form ──────────────
  val dec1 = nrss.map(n => Module(n.getDecoder(expWidth, sigWidth, Some(ie), Some(ifrac))))
  val dec2 = nrss.map(n => Module(n.getDecoder(expWidth, sigWidth, Some(ie), Some(ifrac))))
  dec1.foreach(_.io.binary := in1)
  dec2.foreach(_.io.binary := in2)

  val fp1 = Wire(new FloatingPoint(ie, ifrac, false))
  val fp2 = Wire(new FloatingPoint(ie, ifrac, false))
  fp1 := selById(dec1.head.io.result, dec1.map(_.io.result))
  fp2 := selById(dec2.head.io.result, dec2.map(_.io.result))

  // ── Value-producing arithmetic on the single shared core ────────────────────
  val i2f = Module(new IntegerToFloatingPoint(32, ie, ifrac))
  i2f.io.integer := in1(31, 0)
  i2f.io.sign    := true.B

  val fp_result = Wire(new FloatingPoint(ie, ifrac, false))
  fp_result := fp1 + fp2
  when(io.op === NRSOp.SUB)          { fp_result := fp1 - fp2 }
  when(io.op === NRSOp.MUL)          { fp_result := fp1 * fp2 }
  when(io.op === NRSOp.DIV)          { fp_result := fp1 / fp2 }
  when(io.op === NRSOp.SQRT)         { fp_result := fp1.sqrt }
  when(io.op === NRSOp.CVT_FROM_INT) { fp_result := i2f.io.result }

  // Encode the shared result through every NRS, select by NRS_ID.
  val enc = nrss.map(n => Module(n.getEncoder(expWidth, sigWidth, Some(ie), Some(ifrac))))
  enc.foreach { e => e.io.floatingPoint := fp_result; setRound(e) }
  val arith_bits = selById(enc.head.io.binary, enc.map(_.io.binary))

  // ── Comparison ──────────────────────────────────────────────────────────────
  val cmp = Module(new FloatingPointComparator(ie, ifrac))
  cmp.io.operand1 := fp1
  cmp.io.operand2 := fp2
  val feq_bits = Mux(cmp.io.e,  1.U(totalWidth.W), 0.U(totalWidth.W))
  val flt_bits = Mux(cmp.io.l,  1.U(totalWidth.W), 0.U(totalWidth.W))
  val fle_bits = Mux(cmp.io.le, 1.U(totalWidth.W), 0.U(totalWidth.W))

  // ── Min / Max: return the selected operand's original encoding ──────────────
  val min_bits = Mux(cmp.io.le, in1, in2)
  val max_bits = Mux(cmp.io.ge, in1, in2)

  // ── Sign injection (bit-level on the encoded value) ─────────────────────────
  val signIdx = nrss.head.getSignIdx(expWidth, sigWidth)  // exp+sig-1 = 31, same for all
  val s1 = in1(signIdx)
  val s2 = in2(signIdx)
  val sgnj_bits  = Cat(s2,      in1(totalWidth - 2, 0))
  val sgnjn_bits = Cat(!s2,     in1(totalWidth - 2, 0))
  val sgnjx_bits = Cat(s1 ^ s2, in1(totalWidth - 2, 0))

  // ── NRS -> signed int32 (truncating) ────────────────────────────────────────
  val f2i = Module(new FloatingPointToInteger(32, ie, ifrac))
  f2i.io.floatingPoint := fp1
  f2i.io.signOut       := true.B
  f2i.io.roundingType  := 1.U // RoundZero
  val cvt_to_int_bits = f2i.io.integer

  // ── Final result mux ────────────────────────────────────────────────────────
  val out = WireDefault(arith_bits)  // ADD/SUB/MUL/DIV/SQRT/CVT_FROM_INT
  when(io.op === NRSOp.FEQ)        { out := feq_bits }
  when(io.op === NRSOp.FLT)        { out := flt_bits }
  when(io.op === NRSOp.FLE)        { out := fle_bits }
  when(io.op === NRSOp.FMIN)       { out := min_bits }
  when(io.op === NRSOp.FMAX)       { out := max_bits }
  when(io.op === NRSOp.SGNJ)       { out := sgnj_bits }
  when(io.op === NRSOp.SGNJN)      { out := sgnjn_bits }
  when(io.op === NRSOp.SGNJX)      { out := sgnjx_bits }
  when(io.op === NRSOp.CVT_TO_INT) { out := cvt_to_int_bits }

  val res = Wire(new FPResult)
  res.data := out  // raw 32-bit NRS encoding; writeback NaN-boxes it
  res.exc  := 0.U

  io.out := Pipe(io.in.valid, res, latency)
}
