#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

//
// ───────────────────────────────────────────────
//   VM CONFIG
// ───────────────────────────────────────────────
//

// 256 регистров (подходящий размер)
#define REG_COUNT 256

// 128 инструкций
#define OPCODE_COUNT 128

// декодирование операндов
#define MEM_SIZE (64 * 1024 * 1024 / sizeof(uint32_t)) // 64 МБ

// память VM
extern uint32_t *mem;

// регистры
extern uint32_t reg[REG_COUNT];

// стартовый адрес
extern uint32_t PC_START;

// чтение/запись памяти
#define mr(addr)        (mem[(uint32_t)(addr)])
#define mw(addr, val)   (mem[(uint32_t)(addr)] = (uint32_t)(val))

// указатель на функцию-операцию
typedef void (*op_ex_f)(uint32_t instruction);

// таблица всех операций
extern op_ex_f op_ex[OPCODE_COUNT];

#endif
