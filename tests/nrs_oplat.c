/* nrs_oplat.c — hardware latency per NRS operation (Posit es2), cycle-accurate.
 * Dependent chains keep each op on the critical path; value stays bounded so
 * no overflow. rdcycle/rdinstret with memory clobber; per-op = total/K. */
#include <stdio.h>
#include <stdint.h>
static inline uint32_t f2b(float f){uint32_t u;__builtin_memcpy(&u,&f,4);return u;}
static inline float    b2f(uint32_t u){float f;__builtin_memcpy(&f,&u,4);return f;}
static inline uint64_t rdc(void){uint64_t c;__asm__ volatile("rdcycle %0":"=r"(c)::"memory");return c;}

#define OP(name,opnum) static inline uint32_t name(uint32_t a,uint32_t b){\
  float r;asm volatile(".insn r 0x2B,3,"#opnum",%0,%1,%2":"=f"(r):"f"(b2f(a)),"f"(b2f(b)));return f2b(r);}
OP(padd,0) OP(psub,1) OP(pmul,2) OP(pdiv,3)
static inline uint32_t psqrt(uint32_t a){float r;asm volatile(".insn r 0x2B,3,4,%0,%1,%1":"=f"(r):"f"(b2f(a)));return f2b(r);}

int main(void){
    const uint32_t ONE=0x40000000u, ZERO=0x0u;
    const int K=2000;
    volatile uint32_t vone=ONE, vzero=ZERO;
    uint32_t r;
    printf("=== hardware latency per NRS operation (Posit es2, K=%d) ===\n", K);

    #define BENCH(label,expr,init) do{ r=(init); uint64_t c0=rdc(); \
        for(int k=0;k<K;k++){ r=(expr); } uint64_t c1=rdc(); \
        printf("  %-6s : %lu cycles/op\n", label, (unsigned long)((c1-c0)/(unsigned)K)); }while(0)

    BENCH("ADD", padd(r,vzero), ONE);     /* r+0 -> stays */
    BENCH("SUB", psub(r,vzero), ONE);     /* r-0 -> stays */
    BENCH("MUL", pmul(r,vone),  ONE);     /* r*1 -> stays */
    BENCH("DIV", pdiv(r,vone),  ONE);     /* r/1 -> stays */
    BENCH("SQRT",psqrt(r),      ONE);     /* sqrt(1)=1 -> stays */
    printf("  (sink %08x)\n", r);
    return 0;
}
