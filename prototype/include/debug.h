#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "vm32.h"

#define NUM_REGS REG_COUNT
#define MEM_LOG_SIZE 256

/* Перенесён на per-vm: logging_enabled, verbose_logging, log_file — в vm_state_t */

/* Функции логирования */
void init_logging(vm_state_t *vm);
void close_logging(vm_state_t *vm);
void log_before(vm_state_t *vm, uint32_t pc, uint32_t instr);
void log_after(vm_state_t *vm, uint32_t pc);
void log_instruction(vm_state_t *vm, uint32_t pc, uint32_t instr);

const char* opcode_name(uint8_t opcode);

#endif /* DEBUG_H */
