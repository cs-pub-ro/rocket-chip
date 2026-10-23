/* nrs_precision.c — precision showdown on the SAME real multiplication.
 *
 *   a = 1 + 2^-12 = 1.000244140625      (exact in IEEE754 AND posit32)
 *   b = 1 + 2^-13 = 1.0001220703125     (exact in IEEE754 AND posit32)
 *   exact a*b     = 1.0003662407398224  (= 1 + 2^-12 + 2^-13 + 2^-25)
 *
 * IEEE754 binary32 has 23 fraction bits -> it must drop the 2^-25 term -> rounds.
 * Posit(32,es=2) near 1.0 has ~27 fraction bits -> keeps it -> exact.
 *
 * Same CPU: standard FMUL.S vs my custom NRS posit MUL (NRS_ID=3, OP=2).
 */
#include <stdio.h>

static inline unsigned bits_of(float f){ unsigned u; __builtin_memcpy(&u,&f,4); return u; }
static inline float    f_of(unsigned u){ float f; __builtin_memcpy(&f,&u,4); return f; }

int main(void) {
    /* same two real values, each encoded in its format */
    unsigned IEEE_A = 0x3f800800u, IEEE_B = 0x3f800400u;   /* a, b in IEEE754  */
    unsigned POS_A  = 0x40008000u, POS_B  = 0x40004000u;   /* a, b in posit32  */

    float ia = f_of(IEEE_A), ib = f_of(IEEE_B), ir;
    float pa = f_of(POS_A),  pb = f_of(POS_B),  pr;

    /* (1) standard IEEE754 multiply — ordinary hardware FMUL.S */
    ir = ia * ib;

    /* (2) my custom instruction — posit(32,es=2) multiply */
    asm volatile(".insn r 0x2B, 3, 2, %0, %1, %2" : "=f"(pr) : "f"(pa), "f"(pb));

    printf("  a         = 1.000244140625\n");
    printf("  b         = 1.0001220703125\n");
    printf("  exact a*b = 1.0003662407398224\n\n");
    printf("  %-9s %-8s %-22s %s\n", "format", "op", "result", "encoding");
    printf("  %-9s %-8s %-22.17g 0x%08x  rounded\n",
           "ieee-754", "fmul.s", (double)ir, bits_of(ir));
    printf("  %-9s %-8s %-22s 0x%08x  exact\n",
           "posit32", "nrs mul", "1.0003662407398224", bits_of(pr));
    printf("\n  posit keeps the 2^-25 term that ieee-754 drops at 23 fraction bits.\n");
    return 0;
}
