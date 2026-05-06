#ifndef VM32_H
#define VM32_H

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

/* project headers */
#include "timers.h"
#include "hashing.h"

/* ===================== CONSTANTS ===================== */

#define VM_MEM_BYTES   (64ULL * 1024ULL * 1024ULL)
#define VM_MEM_WORDS   (VM_MEM_BYTES / 4ULL)

#define REG_COUNT      256
#define OPCODE_COUNT   128
#define MAX_TIMERS     16

#ifndef MEM_LOG_SIZE
#define MEM_LOG_SIZE 256
#endif

/* ===================== FORWARD DECLARATIONS ===================== */

typedef struct hash_table hash_table_t;
struct vm_state;

/* opcode handler */
typedef void (*op_ex_f)(struct vm_state *vm, uint32_t instruction);

/* ===================== CONFIG ===================== */

typedef struct {
    uint32_t clock_rate_hz;
    uint32_t cycle_time_ms;

    bool enable_cycle_check;
    bool enable_hash_check;
    bool enable_tick_timing;

    hash_algorithm_t hash_algo;
} vm_config_t;

/* ===================== PROGRAM STRUCTURES ===================== */


typedef struct {
    char *name;
    uint32_t addr;
    uint32_t size;
} module_info_t;

/* ===================== STATE HELPERS ===================== */

typedef struct {
    uint32_t incremental_hash;
    uint32_t reg_hashes[REG_COUNT];
} incremental_state_t;

/* ===================== COUNTERS ===================== */

typedef struct {
    uint32_t value;
    uint32_t preset;
} CT_Counter;

/* ===================== VM STATE ===================== */

typedef struct vm_state {

    /* --- config --- */
    vm_config_t config;

    /* --- memory --- */
    uint8_t  *mem;
    uint32_t PC_START;

    /* --- registers --- */
    uint32_t PC;
    uint32_t reg[REG_COUNT];

    /* --- execution state --- */
    bool running;
    atomic_bool stop_requested;
    int exit_code;

    /* --- hot reload --- */
    atomic_bool reload_pending;
    char *program_dir;
    uint32_t program_dir_signature;

    /* --- timing --- */
    uint64_t time_ms;
    struct timespec last_tick_time;
    uint64_t instr_ns_accum;

    /* --- integrity --- */
    uint32_t program_hash;
    size_t program_size;

    uint32_t prev_cycle_hash;
    uint32_t cycle_count;

    /* --- modules --- */
    module_info_t *modules;
    size_t module_count;

    /* --- opcode table --- */
    op_ex_f op_ex[OPCODE_COUNT];

    /* --- cache/state --- */
    incremental_state_t incremental;

    /* --- logging --- */
    FILE *log_file;
    bool logging_enabled;
    bool verbose_logging;

    /* --- debug --- */
    void *user_data;

    uint32_t dbg_prev_reg[REG_COUNT];
    uint8_t  dbg_prev_mem[MEM_LOG_SIZE];
    uint32_t dbg_prev_pc;
    uint64_t dbg_instruction_count;

    /* --- tables (vm_tables.c) --- */
    hash_table_t *labels;
    hash_table_t *breakpoints;
    hash_table_t *decoded_cache;

    /* ================= TIMERS ================= */

    IEC_Timer ton_timers[MAX_TIMERS];
    IEC_Timer tof_timers[MAX_TIMERS];
    IEC_Timer tp_timers[MAX_TIMERS];
    IEC_Timer tonr_timers[MAX_TIMERS];
    IEC_Timer tofr_timers[MAX_TIMERS];

    /* ================= COUNTERS ================= */

    CT_Counter ctu_counters[MAX_TIMERS];
    CT_Counter ctd_counters[MAX_TIMERS];
    CT_Counter ctud_counters[MAX_TIMERS];

    /* edge detection */
    bool ctu_prev_input[MAX_TIMERS];
    bool ctd_prev_input[MAX_TIMERS];
    bool ctud_prev_up[MAX_TIMERS];
    bool ctud_prev_down[MAX_TIMERS];
    
    /* generic edges */
    bool edge_prev_rising[MAX_TIMERS];
    bool edge_prev_falling[MAX_TIMERS];
    bool edge_prev_both[MAX_TIMERS];

    /* latches */
    bool rs_latches[MAX_TIMERS];
    bool sr_latches[MAX_TIMERS];

} vm_state_t;

/* ===================== MEMORY ACCESS ===================== */

static inline uint8_t vm_mr8(vm_state_t *vm, uint32_t addr) {
    return vm->mem[addr];
}

static inline void vm_mw8(vm_state_t *vm, uint32_t addr, uint8_t v) {
    vm->mem[addr] = v;
}

static inline uint32_t vm_mr32(vm_state_t *vm, uint32_t addr) {
    return  (uint32_t)vm->mem[addr] |
           ((uint32_t)vm->mem[addr + 1] << 8) |
           ((uint32_t)vm->mem[addr + 2] << 16) |
           ((uint32_t)vm->mem[addr + 3] << 24);
}

static inline void vm_mw32(vm_state_t *vm, uint32_t addr, uint32_t val) {
    vm->mem[addr]     = (uint8_t)(val);
    vm->mem[addr + 1] = (uint8_t)(val >> 8);
    vm->mem[addr + 2] = (uint8_t)(val >> 16);
    vm->mem[addr + 3] = (uint8_t)(val >> 24);
}

#endif /* VM32_H */