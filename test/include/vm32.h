#ifndef VM32_H
#define VM32_H

#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

/* ----------------------------
   Конфигурация VM
   ---------------------------- */
typedef enum {
    HASH_SIMPLE_FNV1A,   // Быстрый хеш для общих целей
    HASH_CRC32           // CRC32 для контроля целостности
} hash_algorithm_t;

typedef struct {
    uint32_t clock_rate_hz;      // Инструкций в секунду (0 = без ограничения)
    uint32_t cycle_time_ms;      // Время цикла ПЛК в миллисекундах
    bool enable_cycle_check;     // Проверка перерасхода времени цикла
    bool enable_hash_check;      // Проверка хеша между циклами
    bool enable_tick_timing;     // Включить ограничение тактовой частоты
    hash_algorithm_t hash_algo;  // Алгоритм хеширования
} vm_config_t;

extern vm_config_t vm_config;

/* Объём памяти в байтах (64 MiB) */
#define VM_MEM_BYTES   (64ULL * 1024ULL * 1024ULL)
#define VM_MEM_WORDS   ((size_t)(VM_MEM_BYTES / 4ULL))

/* Число регистров */
#define REG_COUNT      256

/* Количество опкодов */
#define OPCODE_COUNT   128

/* ----------------------------
   Глобальные переменные
   ---------------------------- */

extern uint32_t PC;
extern uint8_t *mem;
extern uint32_t reg[REG_COUNT];
extern uint32_t PC_START;
extern bool running;

typedef void (*op_ex_f)(uint32_t instruction);
extern op_ex_f op_ex[OPCODE_COUNT];

/* Переменные для работы с циклами */
extern struct timespec last_tick_time;
extern uint32_t cycle_count;
extern uint32_t prev_cycle_hash;

/* ----------------------------
   CRC32 Functions (IEEE 802.3)
   ---------------------------- */

/**
 * Вычисляет CRC32 для блока данных
 * Наиболее надежный метод для контроля целостности
 * 
 * @param data    Указатель на данные
 * @param length  Размер данных в байтах
 * @return        32-битное значение CRC
 */
uint32_t crc32(const void* data, size_t length);

/**
 * Инкрементальное вычисление CRC32 (для больших данных)
 */
uint32_t crc32_begin(void);
uint32_t crc32_update(uint32_t crc, const void* data, size_t length);
uint32_t crc32_finalize(uint32_t crc);

/**
 * Проверка целостности данных по CRC32
 * 
 * @param data          Данные для проверки
 * @param length        Размер данных
 * @param expected_crc  Ожидаемое значение CRC
 * @return              true если CRC совпадает
 */
bool crc32_verify(const void* data, size_t length, uint32_t expected_crc);

/**
 * Специализированные функции для VM32
 */
uint32_t crc32_registers(const uint32_t* registers, size_t count);
uint32_t crc32_memory(const uint8_t* memory, uint32_t start_addr, size_t length);

/**
 * Проверяет целостность регистров
 */
bool verify_registers_integrity(uint32_t expected_hash);

/**
 * Проверяет целостность блока памяти
 */
bool verify_memory_integrity(uint32_t start_addr, size_t length, uint32_t expected_hash);

/* ----------------------------
   Утилиты работы с временем
   ---------------------------- */

void init_timer(void);
void wait_for_tick(void);
long get_elapsed_ms(struct timespec start, struct timespec end);

/* ----------------------------
   Утилиты чтения/записи
   ---------------------------- */

static inline uint8_t mr8(uint32_t addr) {
    return mem[addr];
}

static inline void mw8(uint32_t addr, uint8_t v) {
    mem[addr] = v;
}

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

#endif /* VM32_H */