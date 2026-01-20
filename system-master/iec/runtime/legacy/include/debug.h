#ifndef DEBUG_H
#define DEBUG_H

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

// Функции логирования
void init_logging();
void log_instruction(uint32_t pc, uint32_t instr);
void log_before(uint32_t pc, uint32_t instr);
void log_after(uint32_t pc);
void close_logging();

// Декодирование опкодов для отладки
const char* opcode_name(uint8_t opcode);

#endif