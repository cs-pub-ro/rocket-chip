/* mul_ieee.c — ONE IEEE754 multiply, decimal (hex) output. */
#include <stdio.h>
#include "posit.h"
#define EXACT 1.0001831129193306
int main(void){
    uint32_t A=0x3f800400u, B=0x3f800200u;
    float a=b2f(A), b=b2f(B), r = b2f(A)*b2f(B);
    uint32_t R=f2b(r);
    printf("IEEE754  FMUL\n");
    printf("  a      = %.17g   (0x%08x)\n", (double)a, A);
    printf("  b      = %.17g   (0x%08x)\n", (double)b, B);
    printf("  a*b    = %.17g   (0x%08x)\n", (double)r, R);
    printf("  exact  = %.17g\n", (double)EXACT);
    printf("  error  = %.3e\n", fabs((double)r-(double)EXACT));
    return 0;
}
