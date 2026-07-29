/*
 * nrs_test.c — Milestone 1 end-to-end test for the custom NRS instruction.
 *
 * Custom opcode 0x2B (custom-1), R-type:
 *   funct7 [31:25] = OP      (0 = ADD)
 *   funct3 [14:12] = NRS_ID  (3 = Posit(32, es=2))
 *
 * Operands are raw 32-bit NRS encodings held in FP registers.
 * Runs under the proxy kernel (pk), which proxies printf to the host.
 *
 * Posit(32, es=2):  2.0 = 0x48000000,  4.0 = 0x50000000
 */
#include <stdio.h>
#include <stdint.h>

static inline float    u32f(uint32_t u){ union{uint32_t u; float f;} v; v.u = u; return v.f; }
static inline uint32_t fu32(float    f){ union{uint32_t u; float f;} v; v.f = f; return v.u; }

#define P2_2  UINT32_C(0x48000000)   /* Posit(32,2)  2.0 */
#define P2_4  UINT32_C(0x50000000)   /* Posit(32,2)  4.0 */

int main(void) {
    float a = u32f(P2_2);
    float b = u32f(P2_2);
    float r;

    /* .insn r opcode, funct3(NRS_ID), funct7(OP), rd, rs1, rs2 */
    asm volatile(".insn r 0x2B, 3, 0, %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));

    uint32_t got = fu32(r);
    uint32_t exp = P2_4;
    int pass = (got == exp);

    printf("NRS Posit(32,es=2) ADD : 2 + 2 -> 0x%08lx (expected 0x%08lx)  %s\n",
           (unsigned long)got, (unsigned long)exp, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}
