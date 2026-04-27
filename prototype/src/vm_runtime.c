#define _POSIX_C_SOURCE 200809L

#include "vm_runtime.h"
#include "funcs.h"
#include "debug.h"
#include "hashing.h"
#include "cleanup.h"
#include "hot_reload.h"
#include "loader.h"
#include "vm_helpers.h"
#include "vm_tables.h"

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define MAX_INSTRUCTIONS 100000

static int vm_run_instruction(vm_state_t *vm,
                              module_info_t *mod,
                              uint32_t module_end,
                              uint64_t *instr_count,
                              uint64_t *total_instr_count);

static int vm_run_module(vm_state_t *vm,
                         module_info_t *mod,
                         uint64_t *total_instr_count);

static void vm_update_time_after_instruction(vm_state_t *vm);

/* --------------------------------------------------------------------------
   Обновление виртуального времени после одной инструкции
   -------------------------------------------------------------------------- */
static void vm_update_time_after_instruction(vm_state_t *vm) {
    if (!vm) return;

    if (vm->config.enable_tick_timing && vm->config.clock_rate_hz != 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        vm->time_ms = (uint64_t)now.tv_sec * 1000ULL +
                      (uint64_t)(now.tv_nsec / 1000000ULL);
        return;
    }

    if (vm->config.clock_rate_hz != 0) {
        uint64_t instr_ns = 1000000000ULL / (uint64_t)vm->config.clock_rate_hz;
        vm->instr_ns_accum += instr_ns;

        uint64_t add_ms = vm->instr_ns_accum / 1000000ULL;
        if (add_ms) {
            vm->time_ms += add_ms;
            vm->instr_ns_accum -= add_ms * 1000000ULL;
        }
    }
}

/* --------------------------------------------------------------------------
   Выполнение одной инструкции
   return:
     0  -> продолжаем цикл модуля
     1  -> модуль завершён / нужно выйти из цикла инструкции
    -1  -> критическая ошибка аргументов
   -------------------------------------------------------------------------- */
static int vm_run_instruction(vm_state_t *vm,
                              module_info_t *mod,
                              uint32_t module_end,
                              uint64_t *instr_count,
                              uint64_t *total_instr_count) {
    if (!vm || !mod || !instr_count || !total_instr_count) return -1;

    if (vm->PC >= module_end || vm->PC < mod->addr) {
        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "INFO: Module %s completed (PC=0x%X)\n",
                    mod->name, vm->PC);
        }
        return 1;
    }

    uint32_t instr = vm_mr32(vm, vm->PC);
    if (instr == 0) {
        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "INFO: End of module %s at PC=0x%X\n",
                    mod->name, vm->PC);
        }
        return 1;
    }

    decoded_instr_t *d = vm_decode_instruction(vm, vm->PC);
    if (!d) {
        printf("Cycle %u, Module %s: Failed to decode at PC=0x%X\n",
               vm->cycle_count, mod->name, vm->PC);
        vm->running = false;
        return 1;
    }

    uint8_t opcode = OPC(instr);
    if (opcode >= OPCODE_COUNT || vm->op_ex[opcode] == NULL) {
        printf("Cycle %u, Module %s: Invalid opcode %u at PC=0x%X\n",
               vm->cycle_count, mod->name, opcode, vm->PC);
        vm->running = false;
        return 1;
    }

    uint32_t old_pc = vm->PC;

    log_before(vm, vm->PC, instr);
    vm->op_ex[opcode](vm, d->raw_instr);
    if (vm->PC == old_pc) {
        vm->PC += 4;
    }
    log_after(vm, vm->PC);

    (*instr_count)++;
    (*total_instr_count)++;

    vm_wait_for_tick(vm);
    vm_update_time_after_instruction(vm);

    return 0;
}

/* --------------------------------------------------------------------------
   Выполнение одного модуля
   -------------------------------------------------------------------------- */
static int vm_run_module(vm_state_t *vm,
                         module_info_t *mod,
                         uint64_t *total_instr_count) {
    if (!vm || !mod || !total_instr_count) return -1;
    if (!vm->modules || vm->module_count == 0) return 0;

    if (vm->logging_enabled && vm->log_file) {
        fprintf(vm->log_file, "\n--- Executing module: %s (0x%X) ---\n",
                mod->name, mod->addr);
    }

    vm->running = true;
    vm->PC = mod->addr;

    uint64_t instr_count = 0;
    uint32_t module_end = mod->addr + mod->size;

    while (vm->running && instr_count < MAX_INSTRUCTIONS) {
        int rc = vm_run_instruction(vm, mod, module_end, &instr_count, total_instr_count);
        if (rc != 0) {
            break;
        }
    }

    if (vm->logging_enabled && vm->log_file) {
        fprintf(vm->log_file, "Module %s: executed %lu instructions\n",
                mod->name, (unsigned long)instr_count);
    }

    return 0;
}

/* --------------------------------------------------------------------------
   Основной runtime-цикл
   -------------------------------------------------------------------------- */
int run_program(vm_state_t *vm) {
    if (!vm) return -1;

    struct timespec cycle_start, cycle_end;

    vm_init_timer(vm);
    init_logging(vm);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    vm->time_ms = (uint64_t)now.tv_sec * 1000ULL +
                  (uint64_t)(now.tv_nsec / 1000000ULL);

    /* как и раньше */
    vm->config.hash_algo = HASH_CRC32;

    printf("Starting VM execution with cycle-based execution\n");
    printf("Cycle time: %u ms\n", vm->config.cycle_time_ms);
    printf("Hash check: %s\n", vm->config.enable_hash_check ? "enabled" : "disabled");
    printf("Cycle overrun check: %s\n", vm->config.enable_cycle_check ? "enabled" : "disabled");
    printf("Tick timing: %s\n", vm->config.enable_tick_timing ? "enabled" : "disabled");
    printf("Clock rate: %u Hz\n", vm->config.clock_rate_hz);
    printf("\n");

    while (1) {
        if (vm->program_dir && directory_changed(vm->program_dir, &vm->program_dir_signature)) {
            atomic_store(&vm->reload_pending, true);
        }

        apply_pending_reload(vm);

        if (atomic_load(&vm_stop_requested)) {
            atomic_store(&vm->stop_requested, true);
        }

        vm->cycle_count++;
        clock_gettime(CLOCK_MONOTONIC, &cycle_start);

        if (atomic_load(&vm->stop_requested)) {
            run_cleanups();
            break;
        }

        uint64_t cycle_start_ms = vm->time_ms;

        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "\n=== CYCLE %u START (time_ms=%llu) ===\n",
                    vm->cycle_count, (unsigned long long)vm->time_ms);
        }

        uint32_t start_hash = calculate_registers_hash_ex(
            vm->reg,
            REG_COUNT,
            vm->config.hash_algo
        );

        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "Registers hash at start: 0x%08X\n", start_hash);
        }

        if (vm->config.enable_hash_check && vm->cycle_count == 1) {
            vm->prev_cycle_hash = start_hash;
            if (vm->logging_enabled && vm->log_file) {
                fprintf(vm->log_file, "Initialized prev_cycle_hash = 0x%08X\n",
                        vm->prev_cycle_hash);
            }
        }

        uint64_t total_instr_count = 0;

        for (size_t mod_idx = 0; mod_idx < vm->module_count; ++mod_idx) {
            module_info_t *mod = &vm->modules[mod_idx];
            vm_run_module(vm, mod, &total_instr_count);
        }

        if (!vm->config.enable_tick_timing) {
            uint64_t expected_end = cycle_start_ms + (uint64_t)vm->config.cycle_time_ms;
            if (vm->time_ms < expected_end) {
                vm->time_ms = expected_end;
            }
        }

        update_all_timers(vm);

        uint32_t end_hash = calculate_registers_hash_ex(
            vm->reg,
            REG_COUNT,
            vm->config.hash_algo
        );

        if (vm->config.enable_hash_check) {
            if (vm->cycle_count > 1 && end_hash != vm->prev_cycle_hash) {
                if (vm->log_file) {
                    fprintf(vm->log_file,
                            "!!! HASH MISMATCH: Previous 0x%08X, current 0x%08X\n",
                            vm->prev_cycle_hash, end_hash);
                }
                printf("!!! HASH MISMATCH detected on cycle %u\n", vm->cycle_count);
            }

            uint32_t cur_prog_hash = calculate_memory_hash(vm->mem, vm->PC_START, vm->program_size);
            if (cur_prog_hash != vm->program_hash) {
                if (vm->log_file) {
                    fprintf(vm->log_file, "!!! PROGRAM MEMORY HASH MISMATCH\n");
                }
                printf("!!! PROGRAM MEMORY HASH MISMATCH on cycle %u\n", vm->cycle_count);
            }
        }

        vm->prev_cycle_hash = end_hash;

        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "Registers hash at end: 0x%08X\n", end_hash);
            fprintf(vm->log_file, "time_ms at end: %llu (delta: %llu ms)\n",
                    (unsigned long long)vm->time_ms,
                    (unsigned long long)(vm->time_ms - cycle_start_ms));
        }

        clock_gettime(CLOCK_MONOTONIC, &cycle_end);
        long elapsed_ms = vm_get_elapsed_ms(cycle_start, cycle_end);
        long remaining_ms = (long)vm->config.cycle_time_ms - elapsed_ms;

        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "VM elapsed this cycle: %ld ms\n", elapsed_ms);
            fprintf(vm->log_file, "=== CYCLE %u END: %lu instructions in %ld ms ===\n",
                    vm->cycle_count, (unsigned long)total_instr_count, elapsed_ms);
        }

        printf("Cycle %u: %lu instructions, %ld ms elapsed, time_ms=%llu\n",
               vm->cycle_count,
               (unsigned long)total_instr_count,
               elapsed_ms,
               (unsigned long long)vm->time_ms);

        if (remaining_ms > 0) {
            struct timespec sleep_time;
            sleep_time.tv_sec = remaining_ms / 1000;
            sleep_time.tv_nsec = (remaining_ms % 1000) * 1000000L;
            nanosleep(&sleep_time, NULL);
        } else if (vm->config.enable_cycle_check) {
            printf("!!! WARNING: Cycle %u overrun by %ld ms\n",
                   vm->cycle_count, -remaining_ms);
        }
    }

    close_logging(vm);
    return 0;
}