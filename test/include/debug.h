#ifndef DEBUG_H
#define DEBUG_H

#include "vm32.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define NUM_REGS REG_COUNT
#define MEM_LOG_SIZE 256

// Глобальные переменные для логирования
extern bool logging_enabled;
extern bool verbose_logging;  // НОВЫЙ флаг для детального лога
extern FILE *log_file;

// Функции вывода
void fprintf_binary(FILE *f, uint32_t num);
void fprintf_inst(FILE *f, uint32_t instr);
void fprintf_mem_bytes(FILE *f, uint8_t *mem, uint32_t from, uint32_t to);
void fprintf_mem_nonzero(FILE *f, uint8_t *mem, uint32_t stop);
void fprintf_reg(FILE *f, uint32_t *reg, int idx);
void fprintf_reg_all(FILE *f, uint32_t *reg, int size);

// Функции логирования
void init_logging();
void log_instruction(uint32_t pc, uint32_t instr);
void log_before(uint32_t pc, uint32_t instr);
void log_after(uint32_t pc);
void close_logging();

// Декодирование опкодов для отладки
const char* opcode_name(uint8_t opcode);

#endif