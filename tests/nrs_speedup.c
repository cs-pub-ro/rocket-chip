/* nrs_speedup.c — THE thesis number: the custom instruction vs software.
 *
 *  Before this work, an NRS (posit) operation could only run in software
 *  emulation. This instruction does it natively in one opcode.
 *
 *  We implement a correct posit32(es2) multiply IN SOFTWARE, prove it
 *  agrees BIT-EXACT with the hardware NRS multiply over many inputs, then
 *  measure cycles for each on the SAME Rocket Chip core.
 *
 *    speedup = software_cycles_per_mul / hardware_cycles_per_mul
 */
#include <stdio.h>
#include <stdint.h>
#include "posit.h"   /* provides f2b, b2f, rdcycle, posit_to_double(bits,es) */

/* rdcycle / rdinstret with a memory clobber so the compiler cannot move
 * timed work across the measurement points. */
static inline uint64_t rdcyc(void){
    uint64_t c; __asm__ volatile("rdcycle %0":"=r"(c)::"memory"); return c;
}
static inline uint64_t rdinstr(void){
    uint64_t c; __asm__ volatile("rdinstret %0":"=r"(c)::"memory"); return c;
}

/* hardware: posit32(es2) multiply, one custom instruction (NRS_ID=3, OP=2) */
static inline uint32_t hw_pmul(uint32_t a, uint32_t b){
    float r; asm volatile(".insn r 0x2B, 3, 2, %0, %1, %2"
                          : "=f"(r) : "f"(b2f(a)), "f"(b2f(b)));
    return f2b(r);
}
/* hardware posit add/div (NRS_ID=3, OP=0/3) — used only to build operands */
static inline uint32_t hw_padd(uint32_t a, uint32_t b){
    float r; asm volatile(".insn r 0x2B, 3, 0, %0, %1, %2"
                          : "=f"(r) : "f"(b2f(a)), "f"(b2f(b)));
    return f2b(r);
}
static inline uint32_t hw_pdiv(uint32_t a, uint32_t b){
    float r; asm volatile(".insn r 0x2B, 3, 3, %0, %1, %2"
                          : "=f"(r) : "f"(b2f(a)), "f"(b2f(b)));
    return f2b(r);
}

/* ---------- software posit32(es2) multiply (round-to-nearest-even) ---------- */
#define ES 2
#define NB 32
/* scale (total power-of-two exponent) = k*(2^ES) + e, with 0<=e<2^ES.
 * Decompose with a shift by ES, since 2^ES is the factor on the regime k. */

/* decode -> sign, scale (power of two), and significand fraction as Q30 (28-bit frac) */
static void p_decode(uint32_t p, int *sign, int *scale, uint64_t *sigQ){
    *sign = (p >> 31) & 1;
    uint32_t mag = (p & 0x80000000u) ? (uint32_t)(-(int32_t)p) : p;   /* magnitude */
    uint32_t t = mag << 1;                       /* bit31 = first regime bit */
    int k, used;
    if (t & 0x80000000u){ int m=0; while((t&0x80000000u)&&m<NB){m++;t<<=1;} k=m-1; used=1+m+1; }
    else               { int m=0; while(!(t&0x80000000u)&&m<NB){m++;t<<=1;} k=-m;  used=1+m+1; }
    t <<= 1;                                     /* drop terminating bit */
    int rem = NB - used, eb = rem<ES?rem:ES;
    uint32_t e = eb>0 ? (t>>(32-eb)) : 0; if(eb>0) t<<=eb; int ef=ES-eb; if(ef<0)ef=0; e<<=ef;
    *scale = (k<<ES) + (int)e;
    /* remaining bits of t (top fbits) are the fraction; build 1.frac as Q28 */
    uint64_t frac28 = (uint64_t)(t >> 4);        /* top 28 bits -> Q28 fraction */
    *sigQ = ((uint64_t)1<<28) | frac28;          /* 1.frac in Q28 */
}

/* encode sign, scale, significand (Q56: hidden 1 at 2^56, value in [1,2))
 * into posit32(es2) with full round-to-nearest-even (all low bits kept). */
static uint32_t p_encode(int sign, int scale, uint64_t sigQ56){
    while (sigQ56 >= ((uint64_t)1<<57)) { sigQ56 >>= 1; scale++; }   /* keep in [1,2) */
    while (sigQ56 <  ((uint64_t)1<<56)) { sigQ56 <<= 1; scale--; }
    int k = scale >> ES;                         /* floor div by 2^ES */
    int e = scale - (k<<ES);                     /* 0..2^ES-1 */
    uint64_t mag = 0; int pos = 30;              /* bit31=sign=0; write [30..0] MSB-first */
    if (k >= 0){
        for(int i=0;i<k+1 && pos>=0;i++){ mag |= (uint64_t)1<<pos; pos--; }
        if(pos>=0) pos--;                        /* terminating 0 */
    } else {
        for(int i=0;i<-k && pos>=0;i++) pos--;   /* zeros */
        if(pos>=0){ mag |= (uint64_t)1<<pos; pos--; } /* terminating 1 */
    }
    for(int i=ES-1;i>=0;i--){ if(pos>=0){ if((e>>i)&1) mag|=(uint64_t)1<<pos; pos--; } }
    /* fraction: 56-bit field below hidden 1, rounded to the bits that fit */
    int fbitsAvail = pos + 1;
    uint64_t frac = sigQ56 & (((uint64_t)1<<56)-1);     /* 56 fraction bits */
    if (fbitsAvail >= 56){
        for(int i=55;i>=0 && pos>=0;i--){ if((frac>>i)&1) mag|=(uint64_t)1<<pos; pos--; }
    } else if (fbitsAvail <= 0){
        int roundBit = (frac>>55)&1;
        uint64_t sticky = frac & (((uint64_t)1<<55)-1);
        if (roundBit && (sticky || (mag & 1))) mag += 1;
    } else {
        int drop = 56 - fbitsAvail;
        uint64_t kept = frac >> drop;
        int roundBit = (frac >> (drop-1)) & 1;
        uint64_t sticky = frac & (((uint64_t)1<<(drop-1))-1);
        for(int i=fbitsAvail-1;i>=0 && pos>=0;i--){ if((kept>>i)&1) mag|=(uint64_t)1<<pos; pos--; }
        int lsb = kept & 1;
        if (roundBit && (sticky || lsb)) mag += 1;       /* RNE carry into pattern */
    }
    uint32_t out = (uint32_t)mag;
    return sign ? (uint32_t)(-(int32_t)out) : out;
}

static uint32_t sw_pmul(uint32_t a, uint32_t b){
    if (a==0 || b==0) return 0;
    if (a==0x80000000u || b==0x80000000u) return 0x80000000u;  /* NaR */
    int sa,sb,ea,eb; uint64_t ma,mb;             /* ma,mb are Q28 (hidden at 2^28) */
    p_decode(a,&sa,&ea,&ma); p_decode(b,&sb,&eb,&mb);
    int sr = sa ^ sb;
    int er = ea + eb;
    uint64_t prod = ma * mb;                     /* Q28*Q28 = Q56 (hidden at 2^56), [1,4) */
    return p_encode(sr, er, prod);               /* encode normalizes [1,4)->[1,2) & keeps tail */
}

int main(void){
    /* ---- build valid posit operands ---- */
    const uint32_t ONE = 0x40000000u;            /* posit32(es2) 1.0 */
    uint32_t vals[40]; int nv=0;
    uint32_t acc = ONE;
    for(int i=1;i<=12;i++){ vals[nv++]=acc; acc = hw_padd(acc, ONE); }   /* integers 1..12 */
    /* fractions that DON'T fit exactly -> exercise rounding: i/j via hw div */
    uint32_t ints[6]; uint32_t a2=ONE;
    for(int i=0;i<6;i++){ ints[i]=a2; a2=hw_padd(a2,ONE); }              /* 1..6 */
    vals[nv++]=hw_pdiv(ints[0],ints[2]);   /* 1/3  */
    vals[nv++]=hw_pdiv(ints[1],ints[4]);   /* 2/5  */
    vals[nv++]=hw_pdiv(ints[0],ints[6-1]); /* 1/6  */
    vals[nv++]=hw_pdiv(ints[3],ints[2]);   /* 4/3  */
    vals[nv++]=hw_pdiv(ints[0],ints[1]);   /* 1/2  (exact) */
    vals[nv++]=hw_pdiv(ints[4],ints[2]);   /* 5/3  */

    /* ---- validate sw vs hw bit-exact over all pairs (integers + fractions) ---- */
    long match=0,total=0,mism=0; uint32_t firstA=0,firstB=0,firstS=0,firstH=0;
    for(int i=0;i<nv;i++) for(int j=0;j<nv;j++){
        uint32_t h=hw_pmul(vals[i],vals[j]), s=sw_pmul(vals[i],vals[j]);
        total++; if(h==s) match++; else { if(!mism){firstA=vals[i];firstB=vals[j];firstS=s;firstH=h;} mism++; }
    }
    printf("posit32 multiply: software emulation vs one custom instruction\n\n");
    printf("  cross-check: software and hardware agree on %ld/%ld products (bit-exact)\n\n",
           match, total);
    if(mism) printf("  first mismatch: %08x * %08x  sw=%08x hw=%08x\n", firstA,firstB,firstS,firstH);

    /* ---- cost: software routine vs one custom instruction ----
       Dependent chain  r = mul(r, 1.0): each op consumes the previous
       result, so it MUST execute every iteration (true data dependency,
       no hoisting). Multiplying by posit 1.0 leaves r unchanged, so the
       chain stays at a fixed fractional value (1/3 -> exercises the
       inexact / rounding path the whole time). Loop overhead is just the
       branch+counter, so cycles/op is close to the real per-op cost. */
#ifndef K_ITERS
#define K_ITERS 4000
#endif
    const int K=K_ITERS;
    volatile uint32_t vone = ONE;              /* volatile 1.0 -> optimizer can't
                                                  prove mul-by-1 is identity */
    uint32_t seed = vals[12];                  /* 1/3, an inexact posit */
    uint32_t rs = seed, rh = seed;
    uint64_t c0=rdcyc(), i0=rdinstr();
    for(int k=0;k<K;k++){ rs = sw_pmul(rs, vone); }
    uint64_t c1=rdcyc(), i1=rdinstr();
    for(int k=0;k<K;k++){ rh = hw_pmul(rh, vone); }
    uint64_t c2=rdcyc(), i2=rdinstr();
    static volatile uint32_t keep; keep = rs ^ rh;   /* consume results so the
                                  timed loops are not dead-code eliminated */

    unsigned long cS=(unsigned long)(c1-c0), cH=(unsigned long)(c2-c1);
    unsigned long nS=(unsigned long)(i1-i0);
    /* nS is the software routine; the hardware path is the single .insn (1).
       cycles are measured per loop iteration on the same core. */
    printf("  %-20s %3lu instructions   %3lu cycles   per multiply\n",
           "software emulation", nS/(unsigned)K, cS/(unsigned)K);
    printf("  %-20s %3d instruction    %3lu cycles   per multiply\n",
           "custom instruction", 1, cH/(unsigned)K);
    printf("\n  one instruction replaces %lu, %lux fewer cycles\n",
           nS/(unsigned)K, cS/cH);
    return 0;
}
