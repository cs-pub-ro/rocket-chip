#!/bin/bash
# Demo runner — execute INSIDE the container, record the terminal.
#   docker exec -it nrshgl-riscv-dev bash /workspace/rocket-chip/tests/run_demo.sh
# Pauses (press enter) let you narrate each step over the recording.
PK=/opt/riscv/riscv64-unknown-elf/bin/pk
cd /workspace/rocket-chip/tests
pause(){ read -rp $'\n  --- press [enter] to continue ---\n'; }
run(){ "$@" 2>&1 | grep -vE "Listening|JTAG"; }

clear
echo "############################################################################"
echo "# 1.  The custom instruction is really baked into the binary               #"
echo "#     the standard toolchain can't name it -> shows a raw .4byte word       #"
echo "############################################################################"
riscv64-unknown-elf-objdump -d out/nrs_precision.riscv | grep -B1 -A1 "4byte"
echo "   0x04e7b7ab = opcode 0x2B (custom-1), NRS_ID=3 (Posit es2), OP=2 (MUL)"
pause

echo "############################################################################"
echo "# 2.  PRECISION SHOWDOWN  — the same multiplication, two number formats     #"
echo "#     both operands are EXACT in IEEE754 *and* posit; only the result diffs #"
echo "############################################################################"
run out/emulator_nrs $PK out/nrs_precision.riscv
echo
echo "   IEEE754 rounds the exact product;  my posit instruction is bit-exact."
pause

echo "############################################################################"
echo "# 3.  Correctness across the board:  every format x every operation         #"
echo "############################################################################"
run out/emulator_nrs $PK out/nrs_full_test.riscv | grep -iE "PASS|FAIL|ALL TESTS|[0-9]+ */ *[0-9]+" | tail -4
pause

echo "############################################################################"
echo "# 4.  SAME binary on a CPU WITHOUT my decode  ->  ILLEGAL INSTRUCTION       #"
echo "#     remove my hardware and the exact same bits are rejected               #"
echo "############################################################################"
run out/emulator_stock $PK out/nrs_test.riscv | grep -iE "^pc |illegal|FAILED" | head -3
echo
echo "   trap at the custom instruction -> the hardware I added is what makes it real."
echo
echo "  (waveform of the posit multiply: see wave_precision.png)"
