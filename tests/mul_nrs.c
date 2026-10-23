#include <stdint.h>
static inline uint32_t f2b(float f){uint32_t u;__builtin_memcpy(&u,&f,4);return u;}
static inline float    b2f(uint32_t u){float f;__builtin_memcpy(&f,&u,4);return f;}
volatile uint32_t A = 0x40008000u, B = 0x40004000u, R;
int main(void){
    float r; asm volatile(".insn r 0x2B, 3, 2, %0, %1, %2":"=f"(r):"f"(b2f(A)),"f"(b2f(B)));
    R = f2b(r); return 0;
}
void _start(void){ main(); asm volatile("li a0,0; li a7,93; ecall"); }
