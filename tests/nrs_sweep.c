/* nrs_sweep.c — precision gap vs chain length (graph data for the slide).
 * Same a = 1+2^-12 product chain; report accurate decimal digits for
 * IEEE754 vs posit32(es2) at several chain lengths. Reuses verified decoder. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>

static inline unsigned bits_of(float f){ unsigned u; __builtin_memcpy(&u,&f,4); return u; }
static inline float    f_of(unsigned u){ float f; __builtin_memcpy(&f,&u,4); return f; }
static inline float pmul(float x,float y){ float r; asm volatile(".insn r 0x2B,3,2,%0,%1,%2":"=f"(r):"f"(x),"f"(y)); return r; }

#define ES 2
#define NBITS 32
static double posit_to_double(uint32_t p){
    if(p==0u) return 0.0; if(p==0x80000000u) return NAN;
    int sign=(p>>31)&1u; uint32_t v=sign?(uint32_t)(-(int32_t)p):p; uint32_t t=v<<1;
    int regime,used;
    if(t&0x80000000u){int m=0;while((t&0x80000000u)&&m<NBITS){m++;t<<=1;}regime=m-1;used=1+m+1;}
    else{int m=0;while(!(t&0x80000000u)&&m<NBITS){m++;t<<=1;}regime=-m;used=1+m+1;}
    t<<=1; int rem=NBITS-used; int eb=rem<ES?rem:ES;
    uint32_t e=eb>0?(t>>(32-eb)):0u; if(eb>0)t<<=eb; int ef=ES-eb; if(ef<0)ef=0; e<<=ef;
    int fb=rem-ES; if(fb<0)fb=0; double fr=0.0;
    if(fb>0){uint32_t f=t>>(32-fb); fr=(double)f/(double)(1u<<fb);}
    double val=ldexp(1.0,(1<<ES)*regime)*ldexp(1.0,(int)e)*(1.0+fr);
    return sign?-val:val;
}

int main(void){
    float ia=f_of(0x3f800800u), pa=f_of(0x40008000u); double da=1.0+ldexp(1.0,-12);
    int Ns[]={1,4,16,64,256,1024,4096};
    printf("chain_len  ieee_digits  posit_digits  gap\n");
    for(unsigned i=0;i<sizeof(Ns)/sizeof(Ns[0]);++i){
        int N=Ns[i]; float ir=ia,pr=pa; double dr=da;
        for(int k=0;k<N;++k){ ir=ir*ia; pr=pmul(pr,pa); dr=dr*da; }
        double rie=fabs((double)ir-dr)/fabs(dr), rpo=fabs(posit_to_double(bits_of(pr))-dr)/fabs(dr);
        double die=-log10(rie), dpo=-log10(rpo);
        printf("%8d   %9.2f    %9.2f    %+.2f\n", N+1, die, dpo, dpo-die);
    }
    return 0;
}
