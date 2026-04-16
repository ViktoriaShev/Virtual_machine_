# =========================================================
# Toolchains
# =========================================================
HOST_CC   := gcc
ARM64_CC  := aarch64-linux-gnu-gcc
ARM32_CC  := arm-linux-gnueabihf-gcc

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
# Directories
# =========================================================
SRC_DIR     := src
INC_DIR     := include
ASM_DIR     := asm
TOOLS_DIR   := tools
TEST_DIR    := tests
BUILD_DIR   := build
OBJ_DIR     := $(BUILD_DIR)/obj
BIN_DIR     := $(BUILD_DIR)/bin
TEST_BUILD  := $(BUILD_DIR)/tests

# =========================================================
# Flags
# =========================================================
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -I$(INC_DIR)
LDFLAGS ?=
LDLIBS  := -lm -latomic

# =========================================================
# Sources
# =========================================================
VM_SRCS := $(wildcard $(SRC_DIR)/*.c)
VM_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/vm_$(VM_SUFFIX)_%.o,$(VM_SRCS))

CONVERTER_SRC := $(TOOLS_DIR)/converter.c
CONVERTER_OBJ := $(OBJ_DIR)/converter.o

ASM_SRCS  := $(wildcard $(ASM_DIR)/*.asm)
BIN_FILES := $(patsubst $(ASM_DIR)/%.asm,$(BIN_DIR)/%.bin,$(ASM_SRCS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(TEST_BUILD)/%,$(TEST_SRCS))

# =========================================================
# Targets
# =========================================================
VM_TARGET        := $(BUILD_DIR)/vm32_$(VM_SUFFIX)
CONVERTER_TARGET := $(BUILD_DIR)/converter
VM_LOG           := $(BUILD_DIR)/vm32_$(VM_SUFFIX).log

# =========================================================
# Default
# =========================================================
all: info dirs $(CONVERTER_TARGET) $(VM_TARGET)

info:
	@echo "TARGET     = $(TARGET)"
	@echo "VM_CC      = $(VM_CC)"
	@echo "VM_BINARY  = $(VM_TARGET)"

dirs:
	mkdir -p $(BUILD_DIR) $(OBJ_DIR) $(BIN_DIR) $(TEST_BUILD)

# =========================================================
# Converter (host tool)
# =========================================================
$(CONVERTER_TARGET): $(CONVERTER_OBJ)
	$(HOST_CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/converter.o: $(CONVERTER_SRC) | dirs
	$(HOST_CC) $(CFLAGS) -c $< -o $@

# =========================================================
# VM build
# =========================================================
$(VM_TARGET): $(VM_OBJS)
	$(VM_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(OBJ_DIR)/vm_$(VM_SUFFIX)_%.o: $(SRC_DIR)/%.c | dirs
	$(VM_CC) $(CFLAGS) -c $< -o $@

# =========================================================
# ASM → BIN
# =========================================================
$(BIN_DIR)/%.bin: $(ASM_DIR)/%.asm $(CONVERTER_TARGET)
	./$(CONVERTER_TARGET) $< $@

convert: $(BIN_FILES)

# =========================================================
# Run (host only)
# =========================================================
run: all convert
ifeq ($(TARGET),host)
	./$(VM_TARGET) $(BIN_FILES) > $(VM_LOG)
	@echo "Log written to $(VM_LOG)"
else
	@echo "Built for $(TARGET). Copy $(VM_TARGET) to device."
endif

# =========================================================
# Tests (auto-discovery)
# =========================================================

$(TEST_BUILD)/%: $(TEST_DIR)/%.c $(VM_SRCS) | dirs
	$(HOST_CC) $(CFLAGS) -DUNIT_TEST -o $@ $^ $(LDLIBS)

test: $(TEST_BINS)
	@echo "Running tests..."
	@for t in $(TEST_BINS); do $$t; done

# =========================================================
# Debug
# =========================================================
debug: CFLAGS += -g -O0
debug: clean all

# =========================================================
# Coverage
# =========================================================
coverage: CFLAGS += --coverage -O0 -g
coverage: LDFLAGS += --coverage
coverage: clean test

coverage-report:
	lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage.info
	genhtml $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage

# =========================================================
# Clean
# =========================================================
clean:
	rm -rf $(BUILD_DIR)
	rm -rf coverage.info out-coverage

.PHONY: all clean convert run debug test coverage coverage-report

# === HOT RELOAD TEST ===
HOT_RELOAD_CFLAGS = -DVM_NO_MAIN -std=c11 -O2 -Wall -Wextra -Iinclude

$(OBJ_DIR)/vm_host_vm32.o: $(SRC_DIR)/vm32.c | dirs
	$(VM_CC) $(HOT_RELOAD_CFLAGS) -c $< -o $@


# =========================================================
# HOT RELOAD TEST (separate build)
# =========================================================

# =========================================================
# HOT RELOAD TEST (separate build)
# =========================================================

HOT_RELOAD_TEST := $(BIN_DIR)/hot_reload_test

HOT_RELOAD_CFLAGS := -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -DVM_NO_MAIN

HOT_RELOAD_VM_OBJ := $(OBJ_DIR)/vm32_hot.o
HOT_RELOAD_TEST_OBJ := $(OBJ_DIR)/hot_reload_test.o

HOT_RELOAD_OBJS := \
	$(OBJ_DIR)/vm_host_cleanup.o \
	$(OBJ_DIR)/vm_host_debug.o \
	$(OBJ_DIR)/vm_host_funcs.o \
	$(OBJ_DIR)/vm_host_hashing.o \
	$(OBJ_DIR)/vm_host_loader.o \
	$(OBJ_DIR)/vm_host_timers.o \
	$(OBJ_DIR)/vm_host_vm_tables.o \
	$(HOT_RELOAD_VM_OBJ) \
	$(HOT_RELOAD_TEST_OBJ)

# VM core compiled WITHOUT main for test
$(HOT_RELOAD_VM_OBJ): $(SRC_DIR)/vm32.c | dirs
	$(VM_CC) $(HOT_RELOAD_CFLAGS) -c $< -o $@

# test file
$(HOT_RELOAD_TEST_OBJ): $(TEST_DIR)/hot_reload_test.c | dirs
	$(VM_CC) $(HOT_RELOAD_CFLAGS) -c $< -o $@

# link
$(HOT_RELOAD_TEST): $(HOT_RELOAD_OBJS)
	mkdir -p $(BIN_DIR)
	$(VM_CC) -o $@ $^ -lm -latomic

hot_reload_test: $(HOT_RELOAD_TEST)