#ifndef VM32_H
#define VM32_H

/* POSIX флаги для clock_gettime/usleep должны быть определены в .c
   перед включением системных заголовков:
   #define _POSIX_C_SOURCE 199309L
   #include "vm32.h"
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> /* для memcpy если нужно */
#include <stddef.h>

typedef struct {
    uint32_t clock_rate_hz;    // Инструкций в секунду (если нужно)
    uint32_t cycle_time_ms;    // Время цикла ПЛК
    bool enable_cycle_check;     // Проверка перерасхода времени цикла
    bool enable_hash_check;      // Проверка хеша между циклами
} vm_config_t;

extern vm_config_t vm_config;
uint32_t simple_hash(const void* data, size_t length);                   // Вычисление хеша данных
uint32_t calculate_registers_hash();   
/* ----------------------------
   Конфигурация VM (настраиваемая)
   ---------------------------- */
#define CLOCK_RATE 100  // 100 инструкций/сек

#define CYCLE_TIME_MS 1000  // 1 секунда на цикл

/* Объём памяти в байтах (64 MiB) */
#define VM_MEM_BYTES   (64ULL * 1024ULL * 1024ULL)

/* Количество 32-бит слов в памяти (для удобства вычислений) */
#define VM_MEM_WORDS   ((size_t)(VM_MEM_BYTES / 4ULL))

/* число регистров */
#define REG_COUNT      256

/* количество опкодов (максимум, можно увеличить) */
#define OPCODE_COUNT   128

/* PC теперь ОТДЕЛЬНАЯ переменная, не в массиве регистров! */
extern uint32_t PC;

/* ----------------------------
   Глобальные переменные (extern)
   ---------------------------- */

/* Память VM — теперь БАЙТОВАЯ! (каждый адрес — байт).
   Реализацию (выделение) делайте в одном .c: uint8_t *mem = NULL;
*/
extern uint8_t *mem;

/* регистры (32-битные) */
extern uint32_t reg[REG_COUNT];

/* стартовый адрес PC (в байтах) */

extern uint32_t PC_START;
extern bool running;

/* указатель на таблицу функций-операций */
typedef void (*op_ex_f)(uint32_t instruction);
extern op_ex_f op_ex[OPCODE_COUNT];

/* ----------------------------
   Утилиты хеширования
   ---------------------------- */

/* Вычисляет хеш блока данных (FNV-1a алгоритм) */
uint32_t simple_hash(const void* data, size_t length);

/* Вычисляет хеш состояния всех регистров */
uint32_t calculate_registers_hash(void);

/* Вычисляет хеш блока памяти */
uint32_t calculate_memory_hash(uint32_t start_addr, size_t length);

/* ----------------------------
   Утилиты чтения/записи (байты и слова)
   ---------------------------- */

/* Безопасные чтение/запись 8-бит */
static inline uint8_t mr8(uint32_t addr) {
    /* безпроверочная; если нужен bounds-check — добавьте вручную */
    return mem[addr];
}
static inline void mw8(uint32_t addr, uint8_t v) {
    mem[addr] = v;
}

/* Чтение 32-битного слова little-endian, по байтовому буферу.
   Используется явная сборка, чтобы не полагаться на выравнивание и порядок байт.
   (Если ваша среда big-endian — поменяйте порядок или используйте memcpy+ntohl.)
*/
static inline uint32_t mr32(uint32_t addr) {
    uint32_t b0 = (uint32_t)mem[addr + 0];
    uint32_t b1 = (uint32_t)mem[addr + 1];
    uint32_t b2 = (uint32_t)mem[addr + 2];
    uint32_t b3 = (uint32_t)mem[addr + 3];
    return (b0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

/* Запись 32-битного слова little-endian */
static inline void mw32(uint32_t addr, uint32_t val) {
    mem[addr + 0] = (uint8_t)(val & 0xFF);
    mem[addr + 1] = (uint8_t)((val >> 8) & 0xFF);
    mem[addr + 2] = (uint8_t)((val >> 16) & 0xFF);
    mem[addr + 3] = (uint8_t)((val >> 24) & 0xFF);
}

/* Полезный макрос — количество байтов памяти */
#define MEM_BYTES (VM_MEM_BYTES)

/* Полезный макрос — число 32-бит слов */
#define MEM_WORDS (VM_MEM_WORDS)


#endif
