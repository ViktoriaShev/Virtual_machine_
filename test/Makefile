# Компилятор и флаги
CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS := -lm -latomic

# Директории
BUILD_DIR := build
ASM_DIR   := asm
BIN_DIR   := $(BUILD_DIR)/bin

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
# ASM → BIN (автоматически все файлы)
# -------------------------------
ASM_SRCS := $(wildcard $(ASM_DIR)/*.asm)
BIN_FILES := $(patsubst $(ASM_DIR)/%.asm,$(BIN_DIR)/%.bin,$(ASM_SRCS))

VM_LOG  := $(BUILD_DIR)/vm32_log.txt

# -------------------------------
# Основные цели
# -------------------------------
all: $(BUILD_DIR) $(BIN_DIR) $(VM_TARGET) $(CONVERTER_TARGET)

# Создание директорий
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

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
# Конвертация ASM → BIN (правило для каждого файла)
# -------------------------------
$(BIN_DIR)/%.bin: $(ASM_DIR)/%.asm $(CONVERTER_TARGET) | $(BIN_DIR)
	./$(CONVERTER_TARGET) $< $@

# Цель для конвертации всех ASM
convert: $(BIN_FILES)
	@echo "✓ Converted $(words $(BIN_FILES)) ASM files to BIN:"
	@ls -1 $(BIN_DIR)

# -------------------------------
# Запуск VM (передаем все .bin файлы как аргументы)
# -------------------------------
vm: $(VM_TARGET) convert
	./$(VM_TARGET) $(BIN_FILES) > $(VM_LOG)

# -------------------------------
# Полный цикл: build → convert → run
# -------------------------------
run: all convert vm
	@echo "=== VM32 execution finished ==="
	@echo "Binaries: $(BIN_DIR)/"
	@echo "Log:      $(VM_LOG)"

# -------------------------------
# Debug сборка
# -------------------------------
debug: CFLAGS += -g -O0
debug: clean all
watch:
	while true; do \
		inotifywait -e modify $(ASM_DIR)/*.asm; \
		make convert; \
	done
# -------------------------------
# Очистка
# -------------------------------
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean debug convert vm run