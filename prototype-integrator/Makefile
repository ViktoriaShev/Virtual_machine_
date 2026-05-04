# =========================================================
# Toolchains
# =========================================================
HOST_CC   := gcc
ARM64_CC  := aarch64-linux-gnu-gcc
ARM32_CC  := arm-linux-gnueabihf-gcc

TARGET ?= host

ifeq ($(TARGET),host)
    CC := $(HOST_CC)
    SUFFIX := host
else ifeq ($(TARGET),arm64)
    CC := $(ARM64_CC)
    SUFFIX := arm64
else ifeq ($(TARGET),arm32)
    CC := $(ARM32_CC)
    SUFFIX := arm32
else
    $(error Unknown TARGET=$(TARGET))
endif

# =========================================================
# Directories
# =========================================================
SRC_DIR   := src
INC_DIR   := include
ASM_DIR   := asm
TOOLS_DIR := tools
TEST_DIR  := tests

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)/bin
TEST_BIN_DIR := $(BUILD_DIR)/tests

# =========================================================
# Flags
# =========================================================
CFLAGS  := -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -I$(TEST_DIR)
LDFLAGS :=
LDLIBS  := -lm -latomic

# =========================================================
# Sources
# =========================================================
VM_SRCS := $(wildcard $(SRC_DIR)/*.c)

# --- разделяем main ---
MAIN_SRC := $(SRC_DIR)/main.c
VM_CORE_SRCS := $(filter-out $(MAIN_SRC), $(VM_SRCS))

# =========================================================
# Objects
# =========================================================
VM_CORE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/core_%.o,$(VM_CORE_SRCS))
MAIN_OBJ     := $(OBJ_DIR)/main.o

# =========================================================
# Converter
# =========================================================
CONVERTER_SRC := $(TOOLS_DIR)/converter.c
CONVERTER_OBJ := $(OBJ_DIR)/converter.o
CONVERTER_BIN := $(BUILD_DIR)/converter

# =========================================================
# ASM → BIN
# =========================================================
ASM_SRCS  := $(wildcard $(ASM_DIR)/*.asm)
BIN_FILES := $(patsubst $(ASM_DIR)/%.asm,$(BIN_DIR)/%.bin,$(ASM_SRCS))

# =========================================================
# Targets
# =========================================================
VM_BIN := $(BUILD_DIR)/vm32_$(SUFFIX)
HOT_RELOAD_BIN := $(BIN_DIR)/hot_reload_test

# =========================================================
# Default
# =========================================================
all: dirs $(CONVERTER_BIN) $(VM_BIN)

# =========================================================
# Directories
# =========================================================
dirs:
	mkdir -p $(BUILD_DIR) $(OBJ_DIR) $(BIN_DIR) $(TEST_BIN_DIR)

# =========================================================
# Core VM (без main)
# =========================================================
$(OBJ_DIR)/core_%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -DVM_NO_MAIN -c $< -o $@

# =========================================================
# Main (CLI)
# =========================================================
$(MAIN_OBJ): $(MAIN_SRC) | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# =========================================================
# VM binary
# =========================================================
$(VM_BIN): $(VM_CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# =========================================================
# Converter
# =========================================================
$(CONVERTER_BIN): $(CONVERTER_OBJ)
	$(HOST_CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/converter.o: $(CONVERTER_SRC) | dirs
	$(HOST_CC) $(CFLAGS) -c $< -o $@

# =========================================================
# ASM → BIN
# =========================================================
$(BIN_DIR)/%.bin: $(ASM_DIR)/%.asm $(CONVERTER_BIN)
	./$(CONVERTER_BIN) $< $@

convert: $(BIN_FILES)

# =========================================================
# Run
# =========================================================
ARGS ?=

run: all convert
ifeq ($(TARGET),host)
	./$(VM_BIN) $(BIN_DIR) $(ARGS)
else
	@echo "Copy $(VM_BIN) to device"
endif

# =========================================================
# Hot reload test
# =========================================================
$(HOT_RELOAD_BIN): $(VM_CORE_OBJS) $(TEST_DIR)/hot_reload_test.c | dirs
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

hot_reload_test: $(HOT_RELOAD_BIN)

# =========================================================
# Unit tests
# =========================================================
ALL_TEST_SRCS := $(shell find $(TEST_DIR) -type f -name "*.c")
TEST_SRCS := $(filter-out $(TEST_DIR)/hot_reload_test.c,$(ALL_TEST_SRCS))
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(TEST_BIN_DIR)/%,$(TEST_SRCS))

$(TEST_BIN_DIR)/%: $(TEST_DIR)/%.c $(VM_CORE_OBJS) | dirs
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do $$t; done

# =========================================================
# Debug
# =========================================================
debug: CFLAGS := -std=c11 -O0 -g -Wall -Wextra -I$(INC_DIR)
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

.PHONY: all clean run convert test debug coverage coverage-report hot_reload_test dirs