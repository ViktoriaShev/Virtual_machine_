# Компилятор и флаги
CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS := -lm -latomic

# Директория сборки
BUILD_DIR := build

# Исходники для vm32
VM_SRCS := vm32.c funcs.c debug.c hashing.c timers.c cleanup.c
VM_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(VM_SRCS))
VM_TARGET := $(BUILD_DIR)/vm32

# Исходники для converter
CONVERTER_SRCS := converter.c
CONVERTER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CONVERTER_SRCS))
CONVERTER_TARGET := $(BUILD_DIR)/converter

# -------------------------------
# Правила сборки
# -------------------------------

all: $(BUILD_DIR) $(VM_TARGET) $(CONVERTER_TARGET)

# Создание директории build, если не существует
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Компиляция бинарника vm32
$(VM_TARGET): $(VM_OBJS)
	$(CC) $(CFLAGS) -o $@ $(VM_OBJS) $(LDFLAGS)

# Компиляция бинарника converter
$(CONVERTER_TARGET): $(CONVERTER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CONVERTER_OBJS)

# Компиляция объектного файла в build/
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка build
clean:
	rm -rf $(BUILD_DIR)

# Сборка для отладки (debug)
debug: CFLAGS += -g -O0
debug: clean all

.PHONY: all clean debug
