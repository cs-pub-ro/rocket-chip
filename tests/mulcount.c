/* mulcount.c — make the 352 -> 1 count verifiable, not just printed.
 * One software posit multiply and one hardware posit multiply, each fenced
 * between two marker instructions (xori x0,x0,imm). The +verbose commit trace
 * can then be sliced between the markers and the executed instructions counted
 * with wc -l / grep -c. The markers retire but do nothing (write x0).
 *
 *   sw multiply : between inst 0x01104013 and 0x02204013
 *   hw multiply : between inst 0x03304013 and 0x04404013
 */
#include <stdio.h>
#include <stdint.h>
#include "posit.h"

static inline uint32_t hw_pmul(uint32_t a, uint32_t b){
    float r; asm volatile(".insn r 0x2B, 3, 2, %0, %1, %2":"=f"(r):"f"(b2f(a)),"f"(b2f(b)));
    return f2b(r);
}
static inline uint32_t hw_padd(uint32_t a, uint32_t b){
    float r; asm volatile(".insn r 0x2B, 3, 0, %0, %1, %2":"=f"(r):"f"(b2f(a)),"f"(b2f(b)));
    return f2b(r);
}
static inline uint32_t hw_pdiv(uint32_t a, uint32_t b){
    float r; asm volatile(".insn r 0x2B, 3, 3, %0, %1, %2":"=f"(r):"f"(b2f(a)),"f"(b2f(b)));
    return f2b(r);
}

#define ES 2
#define NB 32
static void p_decode(uint32_t p, int *sign, int *scale, uint64_t *sigQ){
    *sign = (p >> 31) & 1;
    uint32_t mag = (p & 0x80000000u) ? (uint32_t)(-(int32_t)p) : p;
    uint32_t t = mag << 1; int k, used;
    if (t & 0x80000000u){ int m=0; while((t&0x80000000u)&&m<NB){m++;t<<=1;} k=m-1; used=1+m+1; }
    else               { int m=0; while(!(t&0x80000000u)&&m<NB){m++;t<<=1;} k=-m;  used=1+m+1; }
    t <<= 1; int rem = NB - used, eb = rem<ES?rem:ES;
    uint32_t e = eb>0 ? (t>>(32-eb)) : 0; if(eb>0) t<<=eb; int ef=ES-eb; if(ef<0)ef=0; e<<=ef;
    *scale = (k<<ES) + (int)e;
    uint64_t frac28 = (uint64_t)(t >> 4);
    *sigQ = ((uint64_t)1<<28) | frac28;
}
static uint32_t p_encode(int sign, int scale, uint64_t sigQ56){
    while (sigQ56 >= ((uint64_t)1<<57)) { sigQ56 >>= 1; scale++; }
    while (sigQ56 <  ((uint64_t)1<<56)) { sigQ56 <<= 1; scale--; }
    int k = scale >> ES; int e = scale - (k<<ES);
    uint64_t mag = 0; int pos = 30;
    if (k >= 0){ for(int i=0;i<k+1 && pos>=0;i++){ mag |= (uint64_t)1<<pos; pos--; } if(pos>=0) pos--; }
    else { for(int i=0;i<-k && pos>=0;i++) pos--; if(pos>=0){ mag |= (uint64_t)1<<pos; pos--; } }
    for(int i=ES-1;i>=0;i--){ if(pos>=0){ if((e>>i)&1) mag|=(uint64_t)1<<pos; pos--; } }
    int fbitsAvail = pos + 1; uint64_t frac = sigQ56 & (((uint64_t)1<<56)-1);
    if (fbitsAvail >= 56){ for(int i=55;i>=0 && pos>=0;i--){ if((frac>>i)&1) mag|=(uint64_t)1<<pos; pos--; } }
    else if (fbitsAvail <= 0){ int rb=(frac>>55)&1; uint64_t st=frac&(((uint64_t)1<<55)-1); if(rb&&(st||(mag&1)))mag+=1; }
    else { int drop=56-fbitsAvail; uint64_t kept=frac>>drop; int rb=(frac>>(drop-1))&1;
        uint64_t st=frac&(((uint64_t)1<<(drop-1))-1);
        for(int i=fbitsAvail-1;i>=0 && pos>=0;i--){ if((kept>>i)&1) mag|=(uint64_t)1<<pos; pos--; }
        if(rb&&(st||(kept&1)))mag+=1; }
    uint32_t out=(uint32_t)mag; return sign ? (uint32_t)(-(int32_t)out) : out;
}
static uint32_t sw_pmul(uint32_t a, uint32_t b){
    if (a==0 || b==0) return 0;
    if (a==0x80000000u || b==0x80000000u) return 0x80000000u;
    int sa,sb,ea,eb; uint64_t ma,mb;
    p_decode(a,&sa,&ea,&ma); p_decode(b,&sb,&eb,&mb);
    int sr=sa^sb; int er=ea+eb; uint64_t prod=ma*mb;
    return p_encode(sr, er, prod);
}

#define MARK(imm) asm volatile("xori x0, x0, " #imm ::: "memory")

int main(void){
    uint32_t ONE=0x40000000u;
    uint32_t three = hw_padd(hw_padd(ONE,ONE),ONE);   /* 3   */
    uint32_t four  = hw_padd(three,ONE);              /* 4   */
    volatile uint32_t va = hw_pdiv(ONE, three);       /* 1/3 (inexact) */
    volatile uint32_t vb = hw_pdiv(four, three);      /* 4/3 (inexact) */
    uint32_t rs, rh;

    MARK(0x11); rs = sw_pmul(va, vb); MARK(0x22);     /* software multiply */
    MARK(0x33); rh = hw_pmul(va, vb); MARK(0x44);     /* hardware multiply */

    volatile uint32_t keep = rs ^ rh; (void)keep;
    printf("software result 0x%08x, hardware result 0x%08x, equal=%d\n",
           rs, rh, rs==rh);
    return 0;
}
