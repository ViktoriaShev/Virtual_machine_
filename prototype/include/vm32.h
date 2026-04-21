// vm32.h
#ifndef VM32_H
#define VM32_H
#define _POSIX_C_SOURCE 200809L

#include "timers.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/* Константы */
#define VM_MEM_BYTES   (64ULL * 1024ULL * 1024ULL)
#define VM_MEM_WORDS   ((size_t)(VM_MEM_BYTES / 4ULL))
#define REG_COUNT      256
#define OPCODE_COUNT   128


#ifndef MEM_LOG_SIZE
#define MEM_LOG_SIZE 256
#endif

typedef struct hash_table hash_table_t;

/* Хеш-алгоритм */
typedef enum {
    HASH_SIMPLE_FNV1A,
    HASH_CRC32
} hash_algorithm_t;

/* Конфигурация VM */
typedef struct {
    uint32_t clock_rate_hz;
    uint32_t cycle_time_ms;
    bool enable_cycle_check;
    bool enable_hash_check;
    bool enable_tick_timing;
    hash_algorithm_t hash_algo;
} vm_config_t;

/* decoded instruction */
typedef struct {
    uint32_t raw_instr;
    uint8_t opcode;
    uint8_t ra, rb, rc;
    uint32_t immediate;
    bool has_immediate;
} decoded_instr_t;

/* module info */
typedef struct {
    char *name;
    uint32_t addr;
    uint32_t size;
} module_info_t;

/* incremental state (оставлено) */
typedef struct {
    uint32_t incremental_hash;
    uint32_t reg_hashes[REG_COUNT];
} incremental_state_t;

/* forward declare opcode function type */
struct vm_state;
typedef void (*op_ex_f)(struct vm_state *vm, uint32_t instruction);

/* Forward declarations for timer structures */
#define MAX_TIMERS 16

/*typedef struct {
    bool enabled;
    bool input;
    bool output;
    bool prev_input;
    bool timing;
    struct timespec start;
    uint32_t preset_ms;
    uint32_t ET;
} IEC_Timer;
*/

typedef struct {
    char   *new_name;       // имя нового файла модуля (для отчёта/лога)
    uint8_t *buffer;        // временный буфер с новым образцом
    size_t  size;           // размер нового образа
    uint32_t target_addr;   // адрес замещаемого модуля в памяти VM
    size_t  module_index;   // индекс заменяемого модуля в vm->modules
    uint32_t new_hash;      // CRC32 нового образа (предварительно вычисленный)
} pending_reload_t;

typedef struct {
    uint32_t value;
    uint32_t preset;
} CT_Counter;

/* Основная структура состоянвия VM */
typedef struct vm_state {
    /* конфигурация (копия) */
    vm_config_t config;

    /* память и точка начала загрузки/выполнения */
    uint8_t *mem;            /* выделяется при создании vm */
    uint32_t PC_START;

    /* регистра и регистры */
    uint32_t PC;
    uint32_t reg[REG_COUNT];

    /* состояние исполнения */
    bool running;
    atomic_bool stop_requested; /* упраление остановкой инстанса */
    int exit_code;              /* код выхода при op_exit */
    //pending_reload_t pending_reload;   // данные отложенной замены
    atomic_bool reload_pending;        // атомарный флаг ожидания замены
    char *program_dir;
    uint32_t program_dir_signature;

    /* время/тайминги */
    uint64_t time_ms;
    struct timespec last_tick_time;
    uint64_t instr_ns_accum;

    /* хеш/целостность */
    uint32_t program_hash;
    size_t program_size;
    uint32_t prev_cycle_hash;
    uint32_t cycle_count;

    /* модули */
    module_info_t *modules;
    size_t module_count;

    /* таблица опкодов (можно разделить как const) */
    op_ex_f op_ex[OPCODE_COUNT];

    /* кеш / вспомогательное состояние */
    incremental_state_t incremental;

    /* файлы/логирование (зависит от debug.h) */
    FILE *log_file;
    bool logging_enabled;
    bool verbose_logging;   /* если нужно особо подробный лог */


    /* расширяемость: указатель на пользовательские данные (для тестов) */
    void *user_data;
    uint32_t dbg_prev_reg[REG_COUNT];
    uint8_t  dbg_prev_mem[MEM_LOG_SIZE];
    uint32_t dbg_prev_pc;
    uint64_t dbg_instruction_count;

    /* tables used by vm_tables.c (per-vm) */
    hash_table_t *labels;
    hash_table_t *breakpoints;
    hash_table_t *decoded_cache;

    /* === TIMERS & COUNTERS === */
    /* Таймеры IEC 61131-3 */
    IEC_Timer ton_timers[MAX_TIMERS];   /* TON - On-Delay */
    IEC_Timer tof_timers[MAX_TIMERS];   /* TOF - Off-Delay */
    IEC_Timer tp_timers[MAX_TIMERS];    /* TP - Pulse */
    IEC_Timer tonr_timers[MAX_TIMERS];  /* TONR - Retentive On-Delay */
    IEC_Timer tofr_timers[MAX_TIMERS];  /* TOFR - Retentive Off-Delay */

    /* Счётчики IEC 61131-3 */
    CT_Counter ctu_counters[MAX_TIMERS];  /* CTU - Count Up */
    CT_Counter ctd_counters[MAX_TIMERS];  /* CTD - Count Down */
    CT_Counter ctud_counters[MAX_TIMERS]; /* CTUD - Count Up/Down */

    /* Edge detection для счётчиков */
    bool ctu_prev_input[MAX_TIMERS];
    bool ctd_prev_input[MAX_TIMERS];
    bool ctud_prev_up[MAX_TIMERS];
    bool ctud_prev_down[MAX_TIMERS];

      /* === Generic edge detection / latches === */
    /* Предыдущее состояние входа для generic edge-detect (rising/falling/both) */
    bool edge_prev_input[MAX_TIMERS];

    /* Latches storage (по одному слоту на id 0..MAX_TIMERS-1) */
    bool rs_latches[MAX_TIMERS];  /* RS: reset has priority (R then S) */
    bool sr_latches[MAX_TIMERS];  /* SR: set has priority (S then R) */
    
} vm_state_t;

// TODO: поддержка CALL/RET (в будущих версиях)
// uint32_t pc_stack[256];
// uint32_t pc_stack_ptr;
/* Функции управления жизненным циклом VM */
vm_state_t *vm_create(void);                  /* выделяет vm_state и mem */
void vm_destroy(vm_state_t *vm);              /* освобождает всё */
int vm_init_defaults(vm_state_t *vm);         /* заполняет sane defaults */
int vm_reset(vm_state_t *vm);                 /* очистка регистров/памяти, не освобождая mem */

/* Основной API (замена глобальных функций) */
int load_programs(vm_state_t *vm, const char **fnames, int count);
int run_program(vm_state_t *vm);

/* Совместимые утилиты чтения/записи (инстансные) */
static inline uint8_t vm_mr8(vm_state_t *vm, uint32_t addr) { return vm->mem[addr]; }
static inline void    vm_mw8(vm_state_t *vm, uint32_t addr, uint8_t v) { vm->mem[addr] = v; }

static inline uint32_t vm_mr32(vm_state_t *vm, uint32_t addr) {
    uint32_t b0 = (uint32_t)vm->mem[addr + 0];
    uint32_t b1 = (uint32_t)vm->mem[addr + 1];
    uint32_t b2 = (uint32_t)vm->mem[addr + 2];
    uint32_t b3 = (uint32_t)vm->mem[addr + 3];
    return (b0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static inline void vm_mw32(vm_state_t *vm, uint32_t addr, uint32_t val) {
    vm->mem[addr + 0] = (uint8_t)(val & 0xFF);
    vm->mem[addr + 1] = (uint8_t)((val >> 8) & 0xFF);
    vm->mem[addr + 2] = (uint8_t)((val >> 16) & 0xFF);
    vm->mem[addr + 3] = (uint8_t)((val >> 24) & 0xFF);
}

/* backward compatibility helpers:
   если код ещё использует глобальные mr32/mw32/PC/... можно добавить адаптеры
   или временно определить макросы, но лучше мигрировать код поэтапно. */

/* vm_tables API (не изменяется в сигнатуре, но должен принимать vm_state_t* в реализации) */
decoded_instr_t *vm_decode_instruction(vm_state_t *vm, uint32_t addr);

/* vm_tables lifecycle */
int vm_tables_init(vm_state_t *vm);
void vm_tables_destroy(vm_state_t *vm);

/* timing utils, тоже можно сделать инстансными */
void vm_init_timer(vm_state_t *vm);
void vm_wait_for_tick(vm_state_t *vm);
long vm_get_elapsed_ms(struct timespec start, struct timespec end);

#endif /* VM32_H */