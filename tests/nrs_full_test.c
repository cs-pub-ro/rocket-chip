/*
 * nrs_full_test.c — End-to-end test for the custom NRS instruction:
 * all 6 NRS_IDs x all 15 OPs, on the Rocket Verilator emulator via pk.
 *
 * Custom opcode 0x2B (custom-1), R-type:
 *   funct7[3:0] [28:25] = OP
 *   funct3      [14:12] = NRS_ID
 *
 * Verification strategy:
 *   - value-producing ops (ADD/SUB/MUL/DIV/SQRT/FMIN/FMAX/CVT_FROM_INT) are
 *     checked with the hardware's own FEQ against the canonical expected value,
 *     so the test validates the numeric value independent of encoding details.
 *   - comparisons (FEQ/FLT/FLE) and CVT_TO_INT are checked directly.
 *   - sign-injection (SGNJ/SGNJN/SGNJX) is bit-exact.
 *
 * Canonical bit patterns come from NRSSL (the software reference).
 */
#include <stdio.h>
#include <stdint.h>

#define STR2(x) #x
#define STR(x)  STR2(x)

/* NRS_ID (funct3) */
#define ID_MORRIS            0
#define ID_MORRIS_HEB        1
#define ID_MORRIS_BIAS_HEB   2
#define ID_POSIT_ES2         3
#define ID_POSIT_ES3         4
#define ID_MORRIS_UNARY_HEB  5

/* OP (funct7[3:0]) */
#define OP_ADD           0
#define OP_SUB           1
#define OP_MUL           2
#define OP_DIV           3
#define OP_SQRT          4
#define OP_FEQ           5
#define OP_FLT           6
#define OP_FLE           7
#define OP_FMIN          8
#define OP_FMAX          9
#define OP_SGNJ          10
#define OP_SGNJN         11
#define OP_SGNJX         12
#define OP_CVT_TO_INT    13
#define OP_CVT_FROM_INT  14

static inline float    u32f(uint32_t u){ union{uint32_t u; float f;} v; v.u = u; return v.f; }
static inline uint32_t fu32(float    f){ union{uint32_t u; float f;} v; v.f = f; return v.u; }

static int tests = 0, fails = 0;
static void rec(const char *label, int ok, uint32_t got) {
    tests++;
    if (ok) printf("  PASS  %-16s 0x%08lx\n", label, (unsigned long)got);
    else  { printf("  FAIL  %-16s 0x%08lx\n", label, (unsigned long)got); fails++; }
}

/* Run a binary NRS op: rd = OP(rs1, rs2). */
#define NRS_OP(r, ID, OP, A, B) \
    asm volatile(".insn r 0x2B, " STR(ID) ", " STR(OP) ", %0, %1, %2" \
                 : "=f"(r) : "f"(u32f(A)), "f"(u32f(B)))

/* value op verified via hardware FEQ against canonical EXP */
#define CHK_EQ(label, ID, OP, A, B, EXP) do { \
    float _r; NRS_OP(_r, ID, OP, A, B); \
    uint32_t _rb = fu32(_r); \
    float _q; NRS_OP(_q, ID, OP_FEQ, _rb, (EXP)); \
    rec(label, fu32(_q) == 1u, _rb); \
} while (0)

/* comparison / int result checked directly */
#define CHK_VAL(label, ID, OP, A, B, EXPVAL) do { \
    float _r; NRS_OP(_r, ID, OP, A, B); \
    uint32_t _rb = fu32(_r); \
    rec(label, _rb == (uint32_t)(EXPVAL), _rb); \
} while (0)

#define TEST_NRS(NAME, ID, V2, V4, VN2, VN4) do { \
    printf("\n=== " NAME " (NRS_ID=" STR(ID) ") ===\n"); \
    CHK_EQ ("ADD 2+2=4",    ID, OP_ADD,  V2, V2, V4); \
    CHK_EQ ("SUB 4-2=2",    ID, OP_SUB,  V4, V2, V2); \
    CHK_EQ ("MUL 2*2=4",    ID, OP_MUL,  V2, V2, V4); \
    CHK_EQ ("DIV 4/2=2",    ID, OP_DIV,  V4, V2, V2); \
    CHK_EQ ("SQRT 4=2",     ID, OP_SQRT, V4, V4, V2); \
    CHK_VAL("FEQ 2==2",     ID, OP_FEQ,  V2, V2, 1); \
    CHK_VAL("FEQ 2==4",     ID, OP_FEQ,  V2, V4, 0); \
    CHK_VAL("FLT 2<4",      ID, OP_FLT,  V2, V4, 1); \
    CHK_VAL("FLT 4<2",      ID, OP_FLT,  V4, V2, 0); \
    CHK_VAL("FLE 2<=2",     ID, OP_FLE,  V2, V2, 1); \
    CHK_VAL("FLE 4<=2",     ID, OP_FLE,  V4, V2, 0); \
    CHK_EQ ("FMIN(2,4)=2",  ID, OP_FMIN, V2, V4, V2); \
    CHK_EQ ("FMAX(2,4)=4",  ID, OP_FMAX, V2, V4, V4); \
    CHK_VAL("SGNJ(4,-2)",   ID, OP_SGNJ,  V4, VN2, ((V4)&0x7FFFFFFFu)|((VN2)&0x80000000u)); \
    CHK_VAL("SGNJN(4,-2)",  ID, OP_SGNJN, V4, VN2, ((V4)&0x7FFFFFFFu)|((~(VN2))&0x80000000u)); \
    CHK_VAL("SGNJX(4,-2)",  ID, OP_SGNJX, V4, VN2, (V4)^((VN2)&0x80000000u)); \
    CHK_VAL("CVT_TO_INT 2", ID, OP_CVT_TO_INT,   V2, V2, 2); \
    CHK_EQ ("CVT_FROM_INT", ID, OP_CVT_FROM_INT, 0x00000002u, 0x00000002u, V2); \
} while (0)

int main(void) {
    TEST_NRS("Morris(4,32,RoundZero)",       ID_MORRIS,           0x02000000u, 0x0A000000u, 0x82000000u, 0x8A000000u);
    TEST_NRS("MorrisHEB(4,32,RoundZero)",    ID_MORRIS_HEB,       0x08000000u, 0x10000000u, 0x88000000u, 0x90000000u);
    TEST_NRS("MorrisBiasHEB(4,32,RoundEven)",ID_MORRIS_BIAS_HEB,  0x40000000u, 0x48000000u, 0xC0000000u, 0xC8000000u);
    TEST_NRS("Posit(32,es=2,RoundEven)",     ID_POSIT_ES2,        0x48000000u, 0x50000000u, 0xB8000000u, 0xB0000000u);
    TEST_NRS("Posit(32,es=3,RoundEven)",     ID_POSIT_ES3,        0x44000000u, 0x48000000u, 0xBC000000u, 0xB8000000u);
    TEST_NRS("MorrisUnaryHEB(32,RoundEven)", ID_MORRIS_UNARY_HEB, 0x60000000u, 0x70000000u, 0xE0000000u, 0xF0000000u);

    printf("\n=== %s (%d/%d passed) ===\n",
           fails == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
           tests - fails, tests);
    return fails == 0 ? 0 : 1;
}
