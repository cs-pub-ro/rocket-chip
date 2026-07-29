/* mul_posit3.c — ONE Posit(32,es3) multiply (my custom instruction), decimal output. */
#include <stdio.h>
#include "posit.h"
#define EXACT 1.0001831129193306
int main(void){
    uint32_t A=0x40002000u, B=0x40001000u, R;
    float a=b2f(A), b=b2f(B), r;
    asm volatile(".insn r 0x2B, 4, 2, %0, %1, %2":"=f"(r):"f"(a),"f"(b)); /* NRS_ID=4, OP=2 */
    R=f2b(r);
    printf("Posit(32,es3)  NRS MUL\n");
    printf("  a      = %.17g   (0x%08x)\n", posit_to_double(A,3), A);
    printf("  b      = %.17g   (0x%08x)\n", posit_to_double(B,3), B);
    printf("  a*b    = %.17g   (0x%08x)\n", posit_to_double(R,3), R);
    printf("  exact  = %.17g\n", (double)EXACT);
    printf("  error  = %.3e\n", fabs(posit_to_double(R,3)-(double)EXACT));
    return 0;
}
