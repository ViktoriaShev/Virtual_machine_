#define _POSIX_C_SOURCE 200809L

#include "vm_lifecycle.h"
#include "funcs.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_PC_START 0x3000

/* -------------------------------------------------------------------------
   Локальная таблица опкодов
   ------------------------------------------------------------------------- */
static op_ex_f default_op_ex[OPCODE_COUNT] = {
    op_add, op_sub, op_mul, op_div, op_mod, op_expt, op_abs, op_sqrt, op_ln, op_log,
    op_exp, op_sin, op_cos, op_tan, op_asin, op_acos, op_atan,
    op_and, op_or, op_xor, op_not,
    op_eq, op_ne, op_gt, op_ge, op_lt, op_le,
    op_time, op_date, op_tod, op_dt, op_add_time, op_sub_time,
    op_year, op_month, op_day, op_hour, op_minute, op_second,
    op_len, op_concat, op_left, op_right, op_mid, op_insert, op_delete, op_replace,
    op_ton, op_tof, op_tp,
    op_ctu, op_ctd, op_ctud,
    op_limit, op_sel, op_mux,
    op_rising_edge, op_falling_edge, op_edge_both,
    op_rs_latch, op_sr_latch, op_demux,
    op_jmp, op_jmp_if, op_jmp_if_not,
    op_exit,
    op_halt,
    op_nop
};

/* -------------------------------------------------------------------------
   Внутренние helper'ы
   ------------------------------------------------------------------------- */
static void vm_state_init_opcode_table(vm_state_t *vm);
static void vm_state_init_registers(vm_state_t *vm);
static void vm_state_clear_iec_state(vm_state_t *vm);
static void vm_state_init_common(vm_state_t *vm);
static void vm_state_reset_runtime(vm_state_t *vm);

static void vm_state_init_opcode_table(vm_state_t *vm) {
    if (!vm) return;

    for (size_t i = 0; i < OPCODE_COUNT; ++i) {
        vm->op_ex[i] = (i < (sizeof(default_op_ex) / sizeof(default_op_ex[0])))
                           ? default_op_ex[i]
                           : NULL;
    }
}

static void vm_state_init_registers(vm_state_t *vm) {
    if (!vm) return;

    for (size_t i = 0; i < REG_COUNT; ++i) {
        vm->reg[i] = 0;
    }
}

void vm_perf_stats_init(vm_perf_stats_t *s) {
    if (!s) return;

    s->cycles_total = 0;
    s->instructions_total = 0;

    s->cycle_exec_ns_total = 0;
    s->instr_exec_ns_total = 0;

    s->fetch_ns_total = 0;
    s->decode_ns_total = 0;
    s->dispatch_ns_total = 0;

    s->min_instr_ns = UINT64_MAX;
    s->max_instr_ns = 0;

    s->overrun_cycles = 0;
}

static void vm_state_clear_iec_state(vm_state_t *vm) {
    if (!vm) return;

    memset(vm->edge_prev_rising, 0, sizeof(vm->edge_prev_rising));
    memset(vm->edge_prev_falling, 0, sizeof(vm->edge_prev_falling));
    memset(vm->edge_prev_both, 0, sizeof(vm->edge_prev_both));
    memset(vm->rs_latches, 0, sizeof(vm->rs_latches));
    memset(vm->sr_latches, 0, sizeof(vm->sr_latches));

    for (int t = 0; t < MAX_TIMERS; ++t) {
        vm->ton_timers[t].enabled = false;
        vm->tof_timers[t].enabled = false;
        vm->tp_timers[t].enabled = false;
        vm->tonr_timers[t].enabled = false;
        vm->tofr_timers[t].enabled = false;

        vm->ton_timers[t].input = false;
        vm->ton_timers[t].output = false;
        vm->ton_timers[t].prev_input = false;
        vm->ton_timers[t].timing = false;
        vm->ton_timers[t].preset_ms = 0;
        vm->ton_timers[t].ET = 0;

        vm->tof_timers[t].input = false;
        vm->tof_timers[t].output = false;
        vm->tof_timers[t].prev_input = false;
        vm->tof_timers[t].timing = false;
        vm->tof_timers[t].preset_ms = 0;
        vm->tof_timers[t].ET = 0;

        vm->tp_timers[t].input = false;
        vm->tp_timers[t].output = false;
        vm->tp_timers[t].prev_input = false;
        vm->tp_timers[t].timing = false;
        vm->tp_timers[t].preset_ms = 0;
        vm->tp_timers[t].ET = 0;

        vm->ctu_counters[t].value = 0;
        vm->ctu_counters[t].preset = 0;
        vm->ctd_counters[t].value = 0;
        vm->ctd_counters[t].preset = 0;
        vm->ctud_counters[t].value = 0;
        vm->ctud_counters[t].preset = 0;

        vm->ctu_prev_input[t] = false;
        vm->ctd_prev_input[t] = false;
        vm->ctud_prev_up[t] = false;
        vm->ctud_prev_down[t] = false;
    }
}

static void vm_state_init_common(vm_state_t *vm) {
    if (!vm) return;

    vm->PC_START = DEFAULT_PC_START;
    vm->PC = 0;
    vm->running = true;

    vm->time_ms = 0;
    vm->instr_ns_accum = 0;

    vm->program_hash = 0;
    vm->program_size = 0;
    vm->cycle_count = 0;
    vm->prev_cycle_hash = 0;
    vm->exit_code = 0;

    atomic_store(&vm->stop_requested, false);
    atomic_store(&vm->reload_pending, false);

    vm->config.clock_rate_hz = 100;
    vm->config.cycle_time_ms = 1000;
    vm->config.enable_cycle_check = true;
    vm->config.enable_hash_check = false;
    vm->config.enable_tick_timing = false;
    vm->config.hash_algo = HASH_CRC32;

    
    vm->logging_enabled = true;
    vm->verbose_logging = false;

    vm_state_init_opcode_table(vm);
    vm_state_init_registers(vm);
    vm_state_clear_iec_state(vm);

    vm->dbg_prev_pc = 0;
    vm->dbg_instruction_count = 0;
    memset(vm->dbg_prev_reg, 0, sizeof(vm->dbg_prev_reg));
    memset(vm->dbg_prev_mem, 0, sizeof(vm->dbg_prev_mem));

    vm_perf_stats_init(&vm->perf);
    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);
}

static void vm_state_reset_runtime(vm_state_t *vm) {
    if (!vm) return;

    vm->running = true;
    vm->exit_code = 0;

    atomic_store(&vm->stop_requested, false);
    atomic_store(&vm->reload_pending, false);

    vm->cycle_count = 0;
    vm->prev_cycle_hash = 0;
    vm->instr_ns_accum = 0;
    vm->time_ms = 0;

    vm->PC = vm->PC_START;

    vm_state_init_registers(vm);
    vm_state_clear_iec_state(vm);

    vm->dbg_prev_pc = 0;
    vm->dbg_instruction_count = 0;
    memset(vm->dbg_prev_reg, 0, sizeof(vm->dbg_prev_reg));
    memset(vm->dbg_prev_mem, 0, sizeof(vm->dbg_prev_mem));
    memset(vm->edge_prev_rising, 0, sizeof(vm->edge_prev_rising));
    memset(vm->edge_prev_falling, 0, sizeof(vm->edge_prev_falling));
    memset(vm->edge_prev_both, 0, sizeof(vm->edge_prev_both));
    vm_perf_stats_init(&vm->perf);

    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);
}

/* -------------------------------------------------------------------------
   Жизненный цикл VM
   ------------------------------------------------------------------------- */
vm_state_t *vm_create(void) {
    vm_state_t *vm = (vm_state_t *)calloc(1, sizeof(vm_state_t));
    if (!vm) return NULL;

    vm->mem = (uint8_t *)calloc(1, VM_MEM_BYTES);
    if (!vm->mem) {
        free(vm);
        return NULL;
    }

    vm->program_dir = NULL;
    vm->program_dir_signature = 0;
    vm->modules = NULL;
    vm->module_count = 0;
    vm->log_file = NULL;
    vm->user_data = NULL;

    atomic_init(&vm->stop_requested, false);
    atomic_init(&vm->reload_pending, false);

    vm_state_init_common(vm);

    return vm;
}

void vm_destroy(vm_state_t *vm) {
    if (!vm) return;

    if (vm->modules) {
        for (size_t i = 0; i < vm->module_count; ++i) {
            free(vm->modules[i].name);
        }
        free(vm->modules);
        vm->modules = NULL;
        vm->module_count = 0;
    }

    if (vm->mem) {
        free(vm->mem);
        vm->mem = NULL;
    }

    if (vm->log_file) {
        fclose(vm->log_file);
        vm->log_file = NULL;
    }

    if (vm->program_dir) {
        free(vm->program_dir);
        vm->program_dir = NULL;
    }

    free(vm);
}

/* Можно вызывать для повторной установки базовых значений без пересоздания VM */
int vm_init_defaults(vm_state_t *vm) {
    if (!vm) return -1;

    vm_state_init_common(vm);

    /* ВАЖНО:
       не обнуляй vm->log_file / vm->program_dir / vm->modules здесь,
       если это уже живой объект. Иначе легко потерять владение ресурсами. */
    return 0;
}

/* Сброс только runtime-состояния, без освобождения памяти и без потери конфигурации */
int vm_reset(vm_state_t *vm) {
    if (!vm) return -1;

    vm_state_reset_runtime(vm);
    return 0;
}