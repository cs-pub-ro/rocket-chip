/* nrs_wave.c — minimal single custom-instruction program for waveform capture.
 * One NRS Posit(32,es=2) ADD: 2 + 2 -> expected 0x50000000.
 * No printf (keeps the cycle count / VCD small). Exit code 0 = correct.
 *
 * Custom op 0x2B (custom-1), R-type: funct3=NRS_ID(3=Posit es2), funct7=OP(0=ADD).
 */
int main(void) {
    unsigned a_bits = 0x48000000u;  /* posit es2 encoding of 2 */
    unsigned b_bits = 0x48000000u;  /* posit es2 encoding of 2 */
    unsigned r_bits;
    float a, b, r;

    __builtin_memcpy(&a, &a_bits, 4);
    __builtin_memcpy(&b, &b_bits, 4);

    /* the one instruction we want to see in the waveform */
    asm volatile(".insn r 0x2B, 3, 0, %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));

    __builtin_memcpy(&r_bits, &r, 4);
    return (r_bits == 0x50000000u) ? 0 : 1;
}
