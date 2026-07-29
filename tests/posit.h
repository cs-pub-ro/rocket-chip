/* posit.h — decode a posit(32,es) bit pattern to double, for readable demo output.
 * Mirrors the reference codec in presentation/demo/posit_ieee.py. */
#ifndef POSIT_H
#define POSIT_H
#include <stdint.h>
#include <math.h>

static inline uint32_t f2b(float f){ uint32_t u; __builtin_memcpy(&u,&f,4); return u; }
static inline float    b2f(uint32_t u){ float f; __builtin_memcpy(&f,&u,4); return f; }
static inline uint64_t rdcycle(void){ uint64_t c; __asm__ volatile("rdcycle %0":"=r"(c)); return c; }

static double posit_to_double(uint32_t bits, int es){
    if(bits == 0) return 0.0;
    int sign = (bits >> 31) & 1;
    uint32_t val = bits & 0x7fffffffu;
    if(sign) val = (0x80000000u - val) & 0x7fffffffu;   /* magnitude (two's complement) */
    int b[31];
    for(int i=0;i<31;i++) b[i] = (val >> (30-i)) & 1;
    int first = b[0], i = 0;
    while(i < 31 && b[i] == first) i++;
    int runlen = i;
    int k = (first == 1) ? (runlen - 1) : -runlen;
    i++;                                                /* skip terminating bit */
    int e = 0;
    for(int j=0;j<es;j++){ e = (e<<1) | ((i<31)? b[i] : 0); i++; }
    double frac = 0.0, p = 0.5;
    for(; i<31; i++){ if(b[i]) frac += p; p /= 2.0; }
    double value = ldexp(1.0 + frac, (k << es) + e);
    return sign ? -value : value;
}
#endif
