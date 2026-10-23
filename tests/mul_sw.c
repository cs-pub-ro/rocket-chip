#include <stdint.h>
#include "softposit.h"
volatile uint32_t A = 0x40008000u;   /* posit 1.000244140625 */
volatile uint32_t B = 0x40004000u;   /* posit 1.0001220703125 */
volatile uint32_t R;
int main(void){ R = softposit_mul(A, B); return 0; }
void _start(void){ main(); asm volatile("li a0,0; li a7,93; ecall"); }
