# -------------------------
#   Makefile for VM + Assembler
# -------------------------

CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude

# Основные цели
all: vm32 converter

# -------------------------
#   VM32
# -------------------------
VM_OBJS = vm32.o funcs.o debug.o hashing.o timers.o

vm32: $(VM_OBJS)
	$(CC) $(CFLAGS) -o vm32 $(VM_OBJS) -lm

vm32.o: vm32.c include/vm32.h include/funcs.h include/debug.h
	$(CC) $(CFLAGS) -c vm32.c

funcs.o: funcs.c include/funcs.h include/vm32.h
	$(CC) $(CFLAGS) -c funcs.c

debug.o: debug.c include/debug.h include/vm32.h
	$(CC) $(CFLAGS) -c debug.c

hashing.o: hashing.c include/hashing.h include/vm32.h
	$(CC) $(CFLAGS) -c hashing.c

timers.o: timers.c include/timers.h include/vm32.h
	$(CC) $(CFLAGS) -c timers.c
# -------------------------
#   Assembler (converter)
# -------------------------
converter: converter.o
	$(CC) $(CFLAGS) -o converter converter.o

converter.o: converter.c include/vm32.h
	$(CC) $(CFLAGS) -c converter.c


# -------------------------
#   Доп. цели
# -------------------------

# перекомпиляция всех
rebuild: clean all

# Удаление объектов и бинарников
clean:
	rm -f *.o vm32 converter program.bin

# Удобство: собрать program.asm → program.bin
build-bin: converter program.asm
	./converter program.asm program.bin
	@echo "✓ program.bin создан"

.PHONY: all clean rebuild build-bin
