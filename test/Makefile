# Компилятор и флаги
CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS := -lm -latomic

# Директория сборки
BUILD_DIR := build

# -------------------------------
# Исходники VM32
# -------------------------------
VM_SRCS := vm32.c funcs.c debug.c hashing.c timers.c cleanup.c
VM_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(VM_SRCS))
VM_TARGET := $(BUILD_DIR)/vm32

# -------------------------------
# Исходники конвертера
# -------------------------------
CONVERTER_SRCS := converter.c
CONVERTER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CONVERTER_SRCS))
CONVERTER_TARGET := $(BUILD_DIR)/converter

# -------------------------------
# Программа
# -------------------------------
ASM_SRC := program.asm
BIN_OUT := $(BUILD_DIR)/program.bin
VM_LOG  := $(BUILD_DIR)/vm32_log.txt

# -------------------------------
# Основные цели
# -------------------------------
all: $(BUILD_DIR) $(VM_TARGET) $(CONVERTER_TARGET)

# Создание директории build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# -------------------------------
# Сборка бинарников
# -------------------------------

$(VM_TARGET): $(VM_OBJS)
	$(CC) $(CFLAGS) -o $@ $(VM_OBJS) $(LDFLAGS)

$(CONVERTER_TARGET): $(CONVERTER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CONVERTER_OBJS)

# Компиляция .c → .o
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------------
# Конвертация ASM → BIN
# -------------------------------
convert: $(CONVERTER_TARGET)
	./$(CONVERTER_TARGET) $(ASM_SRC) $(BIN_OUT)

# -------------------------------
# Запуск VM
# -------------------------------
vm: $(VM_TARGET)
	./$(VM_TARGET) $(BIN_OUT) > $(VM_LOG)

# -------------------------------
# Полный цикл: build → convert → run
# -------------------------------
run: all convert vm
	@echo "=== VM32 execution finished ==="
	@echo "Binary: $(BIN_OUT)"
	@echo "Log:    $(VM_LOG)"

# -------------------------------
# Debug сборка
# -------------------------------
debug: CFLAGS += -g -O0
debug: clean all

# -------------------------------
# Очистка
# -------------------------------
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean debug convert vm run
