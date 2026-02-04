// vm32.h
#ifndef VM32_H
#define VM32_H
#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

/* Конфигурация VM */
typedef enum {
    HASH_SIMPLE_FNV1A,
    HASH_CRC32
} hash_algorithm_t;

typedef struct {
    uint32_t clock_rate_hz;
    uint32_t cycle_time_ms;
    bool enable_cycle_check;
    bool enable_hash_check;
    bool enable_tick_timing;
    hash_algorithm_t hash_algo;
} vm_config_t;

extern vm_config_t vm_config;

/* Объём памяти */
#define VM_MEM_BYTES   (64ULL * 1024ULL * 1024ULL)
#define VM_MEM_WORDS   ((size_t)(VM_MEM_BYTES / 4ULL))

#define REG_COUNT      256
#define OPCODE_COUNT   128

/* Глобальные переменные */
extern uint32_t PC;
extern uint8_t *mem;
extern uint32_t reg[REG_COUNT];
extern uint32_t PC_START;
extern bool running;
extern uint64_t time_ms;
extern uint32_t program_hash;
extern size_t program_size;

typedef void (*op_ex_f)(uint32_t instruction);
extern op_ex_f op_ex[OPCODE_COUNT];

/* Переменные для синхронизации/тайминга */
extern struct timespec last_tick_time;
extern uint32_t cycle_count;
extern uint32_t prev_cycle_hash;

typedef struct {
    uint32_t incremental_hash;
    uint32_t reg_hashes[REG_COUNT];
} incremental_state_t;

/* Вспомогательные функции времени */
void init_timer(void);
void wait_for_tick(void);
long get_elapsed_ms(struct timespec start, struct timespec end);

/* Утилиты чтения/записи в память VM */
static inline uint8_t mr8(uint32_t addr) { return mem[addr]; }
static inline void mw8(uint32_t addr, uint8_t v) { mem[addr] = v; }

static inline uint32_t mr32(uint32_t addr) {
    uint32_t b0 = (uint32_t)mem[addr + 0];
    uint32_t b1 = (uint32_t)mem[addr + 1];
    uint32_t b2 = (uint32_t)mem[addr + 2];
    uint32_t b3 = (uint32_t)mem[addr + 3];
    return (b0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static inline void mw32(uint32_t addr, uint32_t val) {
    mem[addr + 0] = (uint8_t)(val & 0xFF);
    mem[addr + 1] = (uint8_t)((val >> 8) & 0xFF);
    mem[addr + 2] = (uint8_t)((val >> 16) & 0xFF);
    mem[addr + 3] = (uint8_t)((val >> 24) & 0xFF);
}

#define MEM_BYTES (VM_MEM_BYTES)
#define MEM_WORDS (VM_MEM_WORDS)

typedef struct {
    uint32_t raw_instr;
    uint8_t opcode;
    uint8_t ra, rb, rc;
    uint32_t immediate;
    bool has_immediate;
} decoded_instr_t;

/* Модули */
typedef struct {
    char *name;
    uint32_t addr;
    uint32_t size;
} module_info_t;

extern module_info_t *modules;
extern size_t module_count;

/* Тестовый экспорт для decode_instruction (реализован в vm_tables.c) */
decoded_instr_t *vm_decode_instruction(uint32_t addr);

/* Прототипы: vm_tables.*
   Реализация в vm_tables.c */
void vm_tables_init(void);
void vm_tables_destroy(void);
void labels_add(const char *name, uint32_t addr);
uint32_t *labels_lookup(const char *name);
void bp_set(uint32_t addr);
void bp_clear(uint32_t addr);
bool bp_is_set(uint32_t addr);

#endif /* VM32_H */
