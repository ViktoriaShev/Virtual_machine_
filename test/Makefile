# =========================================================
# Toolchains
# =========================================================
HOST_CC   := gcc
ARM64_CC  := aarch64-linux-gnu-gcc
ARM32_CC  := arm-linux-gnueabihf-gcc

# По умолчанию собираем VM под host
TARGET ?= host

ifeq ($(TARGET),host)
VM_CC := $(HOST_CC)
VM_SUFFIX := host
endif

ifeq ($(TARGET),arm64)
VM_CC := $(ARM64_CC)
VM_SUFFIX := arm64
endif

ifeq ($(TARGET),arm32)
VM_CC := $(ARM32_CC)
VM_SUFFIX := arm32
endif

# =========================================================
# Flags
# =========================================================
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS := -lm -latomic

# =========================================================
# Directories
# =========================================================
BUILD_DIR := build
ASM_DIR   := asm
BIN_DIR   := $(BUILD_DIR)/bin

# =========================================================
# Sources
# =========================================================
VM_SRCS := vm32.c funcs.c debug.c hashing.c timers.c cleanup.c

# Host tool (assembler / converter)
CONVERTER_SRC := converter.c

# =========================================================
# Targets
# =========================================================
VM_TARGET        := $(BUILD_DIR)/vm32_$(VM_SUFFIX)
CONVERTER_TARGET := $(BUILD_DIR)/converter

VM_OBJS := $(patsubst %.c,$(BUILD_DIR)/vm_$(VM_SUFFIX)_%.o,$(VM_SRCS))
CONVERTER_OBJS := $(BUILD_DIR)/host_converter.o

ASM_SRCS  := $(wildcard $(ASM_DIR)/*.asm)
BIN_FILES := $(patsubst $(ASM_DIR)/%.asm,$(BIN_DIR)/%.bin,$(ASM_SRCS))

VM_LOG := $(BUILD_DIR)/vm32_$(VM_SUFFIX).log

# =========================================================
# Default
# =========================================================
all: info dirs $(CONVERTER_TARGET) $(VM_TARGET)

info:
	@echo "=== Build configuration ==="
	@echo "TARGET     = $(TARGET)"
	@echo "VM_CC      = $(VM_CC)"
	@echo "VM_BINARY  = $(VM_TARGET)"
	@echo "==========================="

dirs:
	mkdir -p $(BUILD_DIR) $(BIN_DIR)

# =========================================================
# Host tool (converter)
# =========================================================
$(CONVERTER_TARGET): $(CONVERTER_OBJS)
	$(HOST_CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/host_converter.o: $(CONVERTER_SRC)
	$(HOST_CC) $(CFLAGS) -c $< -o $@

# =========================================================
# VM (target)
# =========================================================
$(VM_TARGET): $(VM_OBJS)
	$(VM_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/vm_$(VM_SUFFIX)_%.o: %.c
	$(VM_CC) $(CFLAGS) -c $< -o $@

# =========================================================
# ASM → BIN
# =========================================================
$(BIN_DIR)/%.bin: $(ASM_DIR)/%.asm $(CONVERTER_TARGET) | $(BIN_DIR)
	./$(CONVERTER_TARGET) $< $@

convert: $(BIN_FILES)
	@echo "✓ Converted $(words $(BIN_FILES)) ASM files to BIN"

# =========================================================
# Run (ONLY for host)
# =========================================================
run: all convert
ifeq ($(TARGET),host)
	./$(VM_TARGET) $(BIN_FILES) > $(VM_LOG)
	@echo "Log written to $(VM_LOG)"
else
	@echo "VM built for $(TARGET). Copy $(VM_TARGET) to target device to run."
endif

# =========================================================
# Debug
# =========================================================
debug: CFLAGS += -g -O0
debug: clean all

# =========================================================
# Watch ASM changes
# =========================================================
watch:
	while true; do \
		inotifywait -e modify $(ASM_DIR); \
		make convert; \
	done

# =========================================================
# Clean
# =========================================================
clean:
	rm -rf $(BUILD_DIR)

# =========================================================
# Tests
# =========================================================
TEST_CC := $(HOST_CC)
TEST_CFLAGS := $(CFLAGS)
TEST_TARGET := testing/test_timers

# build the timers unit test
$(TEST_TARGET): testing/test_timers.c timers.c | dirs
	@mkdir -p testing
	$(TEST_CC) $(TEST_CFLAGS) -Iinclude -o $@ $^ $(LDFLAGS)

test-timers: $(TEST_TARGET)

test-decode: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_decode testing/test_decode.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_decode

# hashing unit test: compile only hashing.c + helper header
test-hashing: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_hashing testing/test_hashing.c hashing.c $(LDFLAGS)
	./testing/test_hashing

# funcs_alu unit test: compile funcs.c + vm32.c (for reg[], etc.)
test-funcs-alu:
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_funcs_alu testing/test_funcs_alu.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_funcs_alu

test-funcs-jmp: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_funcs_jmp testing/test_funcs_jmp.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_funcs_jmp

test-labels: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_labels testing/test_labels.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_labels

test-funcs-string: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_funcs_string testing/test_funcs_string.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_funcs_string

test-funcs-counters: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_funcs_counters testing/test_funcs_counters.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_funcs_counters

test-program-flow: dirs
	$(VM_CC) $(CFLAGS) -DUNIT_TEST -Iinclude -o testing/test_program_flow testing/test_program_flow.c $(VM_SRCS) $(LDFLAGS)
	./testing/test_program_flow

# run all tests (add more dependencies here later)
test: test-timers test-decode test-hashing test-funcs-alu test-funcs-jmp test-labels test-funcs-counters test-program-flow test-funcs-string
	@echo "Running timers unit tests..."
	./$(TEST_TARGET)

.PHONY: all clean convert run debug watch info dirs test-timers test
