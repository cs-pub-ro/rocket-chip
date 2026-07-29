/* nrs_bench.c — quantitative numbers for the thesis defense.
 *
 *  All on the SAME Rocket Chip core (cycle-accurate Verilator sim).
 *
 *  (A) PRECISION: product chain  a^(N+1),  a = 1 + 2^-12 = 1.000244140625.
 *      'a' is exactly representable in IEEE754 binary32 AND posit32(es2).
 *      Each multiply rounds; posit near 1.0 keeps ~27 fraction bits vs
 *      IEEE's 23, so it tracks the true value more closely.
 *      Truth = same chain in double (52 fraction bits). We report
 *      accurate decimal digits = -log10(relative error) for each.
 *
 *  (B) LATENCY: cycles per multiply, IEEE FMUL.S vs custom NRS posit MUL,
 *      via rdcycle around a K-iteration loop.
 *
 *  A self-validating posit32(es2) decoder lets us read posit results back
 *  into real values; it is checked against known vectors before use.
 */
#include <stdio.h>
#include <math.h>
#include <stdint.h>

static inline unsigned bits_of(float f){ unsigned u; __builtin_memcpy(&u,&f,4); return u; }
static inline float    f_of(unsigned u){ float f; __builtin_memcpy(&f,&u,4); return f; }
static inline unsigned long rdcycle(void){
    unsigned long c; asm volatile("rdcycle %0" : "=r"(c)); return c;
}
/* posit32(es2) multiply via the custom instruction (NRS_ID=3, OP=2) */
static inline float pmul(float x, float y){
    float r; asm volatile(".insn r 0x2B, 3, 2, %0, %1, %2" : "=f"(r) : "f"(x), "f"(y)); return r;
}

/* ---- posit32, es=2 decoder: bits -> double (verified below) ---- */
#define ES 2
#define NBITS 32
static double posit_to_double(uint32_t p){
    if (p == 0u)          return 0.0;
    if (p == 0x80000000u) return NAN;            /* NaR */
    int sign = (p >> 31) & 1u;
    uint32_t v = sign ? (uint32_t)(-(int32_t)p) : p;   /* magnitude */
    /* drop sign bit: shift so the bit right after sign is the MSB */
    uint32_t t = v << 1;                          /* bit31 = first regime bit */
    int regime, used;                             /* used = sign+regime+term bits */
    if (t & 0x80000000u){                         /* regime = run of 1s */
        int m = 0; while ((t & 0x80000000u) && m < NBITS){ m++; t <<= 1; }
        regime = m - 1; used = 1 + m + 1;         /* sign + m ones + terminating 0 */
    } else {                                      /* regime = run of 0s */
        int m = 0; while (!(t & 0x80000000u) && m < NBITS){ m++; t <<= 1; }
        regime = -m;     used = 1 + m + 1;        /* sign + m zeros + terminating 1 */
        t <<= 0;
    }
    t <<= 1;                                       /* consume terminating bit */
    /* exponent: next ES bits from the top of t */
    int rem = NBITS - used;                        /* bits left for exp+fraction */
    int ebits = rem < ES ? rem : ES;
    uint32_t e = ebits > 0 ? (t >> (32 - ebits)) : 0u;
    if (ebits > 0) t <<= ebits;
    int efill = ES - ebits; if (efill < 0) efill = 0;
    e <<= efill;                                   /* missing exp bits are 0 */
    int fbits = rem - ES; if (fbits < 0) fbits = 0;
    double frac = 0.0;
    if (fbits > 0){
        uint32_t f = t >> (32 - fbits);
        frac = (double)f / (double)(1u << fbits);
    }
    double useed_pow = ldexp(1.0, (1 << ES) * regime);  /* useed^regime = 2^(4*regime) */
    double val = useed_pow * ldexp(1.0, (int)e) * (1.0 + frac);
    return sign ? -val : val;
}

static int approx(double a, double b){ return fabs(a-b) <= 1e-12 * fabs(b) + 1e-15; }

int main(void){
    /* --- validate the posit decoder against known vectors --- */
    struct { uint32_t bits; double val; } vec[] = {
        { 0x40008000u, 1.000244140625      },   /* 1 + 2^-12  */
        { 0x40004000u, 1.0001220703125     },   /* 1 + 2^-13  */
        { 0x4000c004u, 1.0003662407398224  },   /* exact product from nrs_precision */
        { 0x40000000u, 1.0                 },
        { 0x48000000u, 2.0                 },   /* posit32(es2): 4k+e=1 -> 2^1 */
        { 0x50000000u, 4.0                 },   /* posit32(es2): 4k+e=2 -> 2^2 */
        { 0xc0000000u, -1.0                },   /* sign handling */
    };
    int ok = 1;
    printf("=== decoder self-check ===\n");
    for (unsigned i = 0; i < sizeof(vec)/sizeof(vec[0]); ++i){
        double d = posit_to_double(vec[i].bits);
        int pass = approx(d, vec[i].val);
        ok &= pass;
        printf("  0x%08x -> %.17g  %s (want %.17g)\n",
               vec[i].bits, d, pass?"OK":"FAIL", vec[i].val);
    }
    if (!ok){ printf("DECODER BROKEN — numbers below are NOT trustworthy.\n"); return 1; }
    printf("decoder trusted.\n\n");

    /* shared 'a' value */
    float  ia = f_of(0x3f800800u);    /* float  1.000244140625 */
    float  pa = f_of(0x40008000u);    /* posit  1.000244140625 */
    double da = 1.0 + ldexp(1.0,-12);

    /* ---- (A) PRECISION ---- */
    const int N = 1000;
    float  ir = ia; float pr = pa; double dr = da;
    for (int k = 0; k < N; ++k){ ir = ir*ia; pr = pmul(pr,pa); dr = dr*da; }

    double ieee_val  = (double)ir;
    double posit_val = posit_to_double(bits_of(pr));
    double rel_ieee  = fabs(ieee_val  - dr)/fabs(dr);
    double rel_posit = fabs(posit_val - dr)/fabs(dr);
    double dig_ieee  = -log10(rel_ieee);
    double dig_posit = -log10(rel_posit);

    printf("=== (A) PRECISION: product chain a^%d, a=1.000244140625 ===\n", N+1);
    printf("  reference (double): %.17g\n", dr);
    printf("  IEEE754  FMUL.S   : %.17g   rel.err %.3e   %.2f digits\n",
           ieee_val,  rel_ieee,  dig_ieee);
    printf("  posit32  NRS MUL  : %.17g   rel.err %.3e   %.2f digits\n",
           posit_val, rel_posit, dig_posit);
    printf("  posit keeps %.2f more accurate decimal digits, same 32 bits.\n\n",
           dig_posit - dig_ieee);

    /* ---- (B) LATENCY ---- */
    const int K = 4000;
    volatile float xi = ia, xp = pa;
    unsigned long t0 = rdcycle();
    for (int k=0;k<K;++k) xi = xi*ia;
    unsigned long t1 = rdcycle();
    for (int k=0;k<K;++k) xp = pmul(xp,pa);
    unsigned long t2 = rdcycle();
    double c_ieee = (double)(t1-t0)/K, c_nrs = (double)(t2-t1)/K;

    printf("=== (B) LATENCY: cycles per multiply (K=%d) ===\n", K);
    printf("  IEEE754 FMUL.S : %.2f cyc/op  (total %lu)\n", c_ieee, t1-t0);
    printf("  NRS posit MUL  : %.2f cyc/op  (total %lu)\n", c_nrs,  t2-t1);
    printf("  ratio          : %.2fx\n", c_nrs/c_ieee);
    printf("\n(sink %08x %08x)\n", bits_of(xi), bits_of(xp));
    return 0;
}
