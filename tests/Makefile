# Build + run NRS custom-instruction tests on the Rocket Verilator emulator via pk.
#
#   make sim                 # build & run nrs_test on DefaultConfig
#   make sim APP=nrs_test    # pick the test
#   make clean

APP        ?= nrs_test
BUILD_DIR   = out

# ── Toolchain ────────────────────────────────────────────────────────────────
RV_GCC_PATH ?= /opt/riscv
CC           = riscv64-unknown-elf-gcc
PK           = $(RV_GCC_PATH)/riscv64-unknown-elf/bin/pk

# newlib + pk proxy all syscalls (printf, exit, ...) — no custom HTIF needed.
CFLAGS = -march=rv64gc -mabi=lp64d -mcmodel=medany -O0 -g

# ── Simulator ────────────────────────────────────────────────────────────────
SIM_PREFIX = /workspace/rocket-chip/out/emulator/freechips.rocketchip.system.TestHarness/freechips.rocketchip.system.
SIMCONFIG  ?= DefaultConfig
SIM         = $(SIM_PREFIX)$(SIMCONFIG)/verilator/elf.dest/emulator

.PHONY: all clean sim
all: $(BUILD_DIR)/$(APP).riscv

$(BUILD_DIR)/%.riscv: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

sim: $(BUILD_DIR)/$(APP).riscv
	$(SIM) $(PK) $<

clean:
	rm -rf $(BUILD_DIR)
