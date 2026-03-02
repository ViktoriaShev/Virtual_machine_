// vm32.c
#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "funcs.h"
#include "debug.h"
#include "hashing.h"
#include "cleanup.h"
#include "timers.h"
#include "vm_tables.h"
#include "loader.h"

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>   // usleep

/* ----------------------------
   Константы
   ---------------------------- */
#define MAX_INSTRUCTIONS 100000
#define DEFAULT_PC_START 0x3000

/* ----------------------------
   Локальная таблица опкодов (шаблон)
   При создании vm эта таблица копируется в vm->op_ex.
   ---------------------------- */
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
    op_nop,
    /* остальное — NULL (по умолчанию) */
};

/* ----------------------------
   Глобальная переменная для сигналов
   (если CLI запустил только один активный vm — handler ставит флаг stop_requested)
   ---------------------------- */
static vm_state_t *g_active_vm_for_signal = NULL;

static void handle_sigterm_global(int sig) {
   (void)sig;
    /* set global flag from cleanup.c */
    atomic_store(&vm_stop_requested, true);
    if (g_active_vm_for_signal) {
        atomic_store(&g_active_vm_for_signal->stop_requested, true);
    }
}

/* ----------------------------
   Жизненный цикл vm
   ---------------------------- */
vm_state_t *vm_create(void) {
    vm_state_t *vm = (vm_state_t *)calloc(1, sizeof(vm_state_t));
    if (!vm) return NULL;

    /* выделяем память VM */
    vm->mem = (uint8_t *)calloc(1, VM_MEM_BYTES);
    if (!vm->mem) {
        free(vm);
        return NULL;
    }

    /* sensible defaults */
    vm->PC_START = DEFAULT_PC_START;
    vm->PC = 0;
    vm->running = true;
    atomic_init(&vm->stop_requested, false);
    vm->time_ms = 0;
    vm->instr_ns_accum = 0;
    vm->program_hash = 0;
    vm->program_size = 0;
    vm->modules = NULL;
    vm->module_count = 0;
    vm->log_file = NULL;
    vm->logging_enabled = true;
    vm->verbose_logging = false;
    vm->log_file = NULL;

    vm->user_data = NULL;

    /* defaults for config (same as раньше) */
    vm->config.clock_rate_hz = 100;
    vm->config.cycle_time_ms = 1000;
    vm->config.enable_cycle_check = true;
    vm->config.enable_hash_check = true;
    vm->config.enable_tick_timing = false;
    vm->config.hash_algo = HASH_CRC32;

    /* заполняем таблицу опкодов (копируем) */
    for (size_t i = 0; i < OPCODE_COUNT; ++i) {
        vm->op_ex[i] = (i < (sizeof(default_op_ex)/sizeof(default_op_ex[0])) ? default_op_ex[i] : NULL);
    }

    /* инициализация регистров */
    for (size_t i = 0; i < REG_COUNT; ++i) vm->reg[i] = 0;
    /* старое поведение: reg[0]=5, reg[1]=3 */
    vm->reg[0] = 5;
    vm->reg[1] = 3;

    /* Инициализация edge/latch поля */
    memset(vm->edge_prev_input, 0, sizeof(vm->edge_prev_input));
    memset(vm->rs_latches, 0, sizeof(vm->rs_latches));
    memset(vm->sr_latches, 0, sizeof(vm->sr_latches));

    /* Инициализация IEC-таймеров/счётчиков (чтобы не было мусора) */
    for (int t = 0; t < MAX_TIMERS; ++t) {
        vm->ton_timers[t].enabled = false;
        vm->tof_timers[t].enabled = false;
        vm->tp_timers[t].enabled = false;
        vm->tonr_timers[t].enabled = false;
        vm->tofr_timers[t].enabled = false;

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
    /* время */
    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);

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
    }

    if (vm->mem) {
        free(vm->mem);
        vm->mem = NULL;
    }

    /* если лог-файл открыт глобально, закрываем (если вы хотите per-vm logs,
       убедитесь, что debug/ logging использует vm->log_file) */
    if (vm->log_file) {
        fclose(vm->log_file);
        vm->log_file = NULL;
    }

    free(vm);
}

/* Можно вызывать для (пере)установки sane defaults без пересоздания структуры */
int vm_init_defaults(vm_state_t *vm) {
    if (!vm) return -1;
    vm->config.clock_rate_hz = 100;
    vm->config.cycle_time_ms = 1000;
    vm->config.enable_cycle_check = true;
    vm->config.enable_hash_check = true;
    vm->config.enable_tick_timing = false;
    vm->config.hash_algo = HASH_CRC32;

    vm->PC_START = DEFAULT_PC_START;
    vm->program_hash = 0;
    vm->program_size = 0;
    vm->cycle_count = 0;
    vm->prev_cycle_hash = 0;
    atomic_store(&vm->stop_requested, false);
    vm->running = true;
    vm->time_ms = 0;
    vm->instr_ns_accum = 0;
    memset(vm->edge_prev_input, 0, sizeof(vm->edge_prev_input));
    memset(vm->rs_latches, 0, sizeof(vm->rs_latches));
    memset(vm->sr_latches, 0, sizeof(vm->sr_latches));

    for (size_t i = 0; i < REG_COUNT; ++i) vm->reg[i] = 0;
    vm->reg[0] = 5;
    vm->reg[1] = 3;

    /* обнулить таймеры/счётчики */
    for (int t = 0; t < MAX_TIMERS; ++t) {
        vm->ton_timers[t].enabled = false;
        vm->tof_timers[t].enabled = false;
        vm->tp_timers[t].enabled = false;
        vm->tonr_timers[t].enabled = false;
        vm->tofr_timers[t].enabled = false;

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

    vm->logging_enabled = true;
    vm->verbose_logging = false;
    vm->log_file = NULL;


    return 0;
}

/* Сброс состояния VM (не освобождает vm или vm->mem) */
int vm_reset(vm_state_t *vm) {
    if (!vm) return -1;

    /* базовые поля */
    atomic_store(&vm->stop_requested, false);
    vm->running = true;
    vm->exit_code = 0;
    vm->cycle_count = 0;
    vm->prev_cycle_hash = 0;
    vm->instr_ns_accum = 0;
    vm->time_ms = 0;

    /* PC */
    vm->PC = vm->PC_START;

    /* очистка регистров (оставляем reg[0]/reg[1] как ваше "старое поведение") */
    for (size_t i = 0; i < REG_COUNT; ++i) vm->reg[i] = 0;
    vm->reg[0] = 5;
    vm->reg[1] = 3;

    /* очистка памяти данных не делаем (опасно) — если нужно, вызывайте отдельно memset(vm->mem, 0, VM_MEM_BYTES) */

    /* timers / IEC structures — нужно обнулить все поля таймеров/счётчиков/edge prev */
    for (int t = 0; t < MAX_TIMERS; ++t) {
        /* IEC_Timer structures */
        vm->ton_timers[t].enabled = false;
        vm->ton_timers[t].input = false;
        vm->ton_timers[t].output = false;
        vm->ton_timers[t].prev_input = false;
        vm->ton_timers[t].timing = false;
        vm->ton_timers[t].preset_ms = 0;
        vm->ton_timers[t].ET = 0;
        vm->tof_timers[t].enabled = false;
        vm->tof_timers[t].input = false;
        vm->tof_timers[t].output = false;
        vm->tof_timers[t].prev_input = false;
        vm->tof_timers[t].timing = false;
        vm->tof_timers[t].preset_ms = 0;
        vm->tof_timers[t].ET = 0;
        vm->tp_timers[t].enabled = false;
        vm->tp_timers[t].input = false;
        vm->tp_timers[t].output = false;
        vm->tp_timers[t].prev_input = false;
        vm->tp_timers[t].timing = false;
        vm->tp_timers[t].preset_ms = 0;
        vm->tp_timers[t].ET = 0;
        vm->tonr_timers[t].enabled = false;
        vm->tofr_timers[t].enabled = false;

        /* counters */
        vm->ctu_counters[t].value = 0;
        vm->ctu_counters[t].preset = 0;
        vm->ctd_counters[t].value = 0;
        vm->ctd_counters[t].preset = 0;
        vm->ctud_counters[t].value = 0;
        vm->ctud_counters[t].preset = 0;

        /* prev inputs */
        vm->ctu_prev_input[t] = false;
        vm->ctd_prev_input[t] = false;
        vm->ctud_prev_up[t] = false;
        vm->ctud_prev_down[t] = false;

        /* edge + latches */
        vm->edge_prev_input[t] = false;
        vm->rs_latches[t] = false;
        vm->sr_latches[t] = false;
    }

    /* last_tick_time */
    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);

    /* сброс отладочного состояния (если нужно) */
    vm->dbg_prev_pc = 0;
    vm->dbg_instruction_count = 0;
    memset(vm->dbg_prev_reg, 0, sizeof(vm->dbg_prev_reg));
    memset(vm->dbg_prev_mem, 0, sizeof(vm->dbg_prev_mem));

    return 0;
}

/* ----------------------------
   Вспомогательные функции времени — инстансные
   ---------------------------- */
void vm_init_timer(vm_state_t *vm) {
    if (!vm) return;
    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);
}

long vm_get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 +
           (end.tv_nsec - start.tv_nsec) / 1000000;
}

void vm_wait_for_tick(vm_state_t *vm) {
    if (!vm) return;
    if (!vm->config.enable_tick_timing || vm->config.clock_rate_hz == 0) {
        return;
    }
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    long interval_ns = 1000000000 / vm->config.clock_rate_hz;
    long elapsed_ns = (current_time.tv_sec - vm->last_tick_time.tv_sec) * 1000000000 +
                      (current_time.tv_nsec - vm->last_tick_time.tv_nsec);

    if (elapsed_ns < interval_ns) {
        long remaining_ns = interval_ns - elapsed_ns;
        struct timespec sleep_time = {0, remaining_ns};
        nanosleep(&sleep_time, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);
}

/* ----------------------------
   Основной цикл выполнения — инстансный
   ---------------------------- */
int run_program(vm_state_t *vm) {
    if (!vm) return -1;

    struct timespec cycle_start, cycle_end;

    vm_init_timer(vm);

    /* Инициализация логгирования (функция из debug.h) — оставляем вызов,
       предполагая, что debug/init logging можно адаптировать к per-vm позже. */
    init_logging(vm);

    struct timespec __now;
    clock_gettime(CLOCK_MONOTONIC, &__now);
    vm->time_ms = (uint64_t)__now.tv_sec * 1000ULL + (uint64_t)(__now.tv_nsec / 1000000ULL);

    /* Принудительные значения (как раньше) */
    vm->config.hash_algo = HASH_CRC32;
    vm->config.enable_hash_check = true;

    printf("Starting VM execution with cycle-based execution\n");
    printf("Cycle time: %u ms\n", vm->config.cycle_time_ms);
    printf("Hash check: %s\n", vm->config.enable_hash_check ? "enabled" : "disabled");
    printf("Cycle overrun check: %s\n", vm->config.enable_cycle_check ? "enabled" : "disabled");
    printf("Tick timing: %s\n", vm->config.enable_tick_timing ? "enabled" : "disabled");
    printf("Clock rate: %u Hz\n", vm->config.clock_rate_hz);
    printf("\n");

    /* локальный PC-stack (пер-вызовный, не делаем полем vm, можно вынести при необходимости) */
        /* локальный PC-stack (пер-вызовный) */
    uint32_t pc_stack[256];
    uint32_t pc_stack_ptr = 0;
    /* Если понадобятся push/pop, используйте прямые операции с pc_stack и pc_stack_ptr:
         if (pc_stack_ptr < sizeof(pc_stack)/sizeof(pc_stack[0])) pc_stack[pc_stack_ptr++] = value;
         if (pc_stack_ptr > 0) { uint32_t v = pc_stack[--pc_stack_ptr]; ... }
       Сейчас push/pop не используются в этом цикле, поэтому никаких вспомогательных функций не требуется. */


    while (1) {

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
            fprintf(vm->log_file, "\n=== CYCLE %u START (time_ms=%llu) ===\n", vm->cycle_count, (unsigned long long)vm->time_ms);
        }

        uint32_t start_hash = calculate_registers_hash(vm->reg, REG_COUNT);
        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "Registers hash at start: 0x%08X\n", start_hash);
        }

        if (vm->config.enable_hash_check && vm->cycle_count == 1) {
            vm->prev_cycle_hash = start_hash;
            if (vm->logging_enabled && vm->log_file) {
                fprintf(vm->log_file, "Initialized prev_cycle_hash = 0x%08X\n", vm->prev_cycle_hash);
            }
        }

        uint64_t total_instr_count = 0;

        for (size_t mod_idx = 0; mod_idx < vm->module_count; ++mod_idx) {
            module_info_t *mod = &vm->modules[mod_idx];

            if (vm->logging_enabled && vm->log_file) {
                fprintf(vm->log_file, "\n--- Executing module: %s (0x%X) ---\n", mod->name, mod->addr);
            }

            vm->running = true;
            vm->PC = mod->addr;
            pc_stack_ptr = 0;

            uint64_t instr_count = 0;
            uint32_t module_end = mod->addr + mod->size;

            while (vm->running && instr_count < MAX_INSTRUCTIONS) {
                if (vm->PC >= module_end || vm->PC < mod->addr) {
                    if (vm->logging_enabled && vm->log_file) {
                        fprintf(vm->log_file, "INFO: Module %s completed (PC=0x%X)\n", mod->name, vm->PC);
                    }
                    break;
                }

                uint32_t instr = vm_mr32(vm, vm->PC);
                if (instr == 0) {
                    if (vm->logging_enabled && vm->log_file) {
                        fprintf(vm->log_file, "INFO: End of module %s at PC=0x%X\n", mod->name, vm->PC);
                    }
                    break;
                }

                decoded_instr_t *d = vm_decode_instruction(vm, vm->PC);
                if (!d) {
                    printf("Cycle %u, Module %s: Failed to decode at PC=0x%X\n",
                           vm->cycle_count, mod->name, vm->PC);
                    vm->running = false;
                    break;
                }

                uint8_t opcode = OPC(instr);
                if (opcode >= OPCODE_COUNT || vm->op_ex[opcode] == NULL) {
                    printf("Cycle %u, Module %s: Invalid opcode %u at PC=0x%X\n",
                           vm->cycle_count, mod->name, opcode, vm->PC);
                    vm->running = false;
                    break;
                }

                uint32_t old_pc = vm->PC;
                log_before(vm,vm->PC, instr);
                vm->op_ex[opcode](vm, d->raw_instr);
                if (vm->PC == old_pc) vm->PC += 4;
                log_after(vm,vm->PC);

                instr_count++;
                vm_wait_for_tick(vm);

                if (vm->config.enable_tick_timing && vm->config.clock_rate_hz != 0) {
                    struct timespec __now;
                    clock_gettime(CLOCK_MONOTONIC, &__now);
                    vm->time_ms = (uint64_t)__now.tv_sec * 1000ULL + (uint64_t)(__now.tv_nsec / 1000000ULL);
                } else if (vm->config.clock_rate_hz != 0) {
                    uint64_t instr_ns = 1000000000ULL / (uint64_t)vm->config.clock_rate_hz;
                    vm->instr_ns_accum += instr_ns;
                    uint64_t add_ms = vm->instr_ns_accum / 1000000ULL;
                    if (add_ms) {
                        vm->time_ms += add_ms;
                        vm->instr_ns_accum -= add_ms * 1000000ULL;
                    }
                }
            }

            total_instr_count += instr_count;

            if (vm->logging_enabled && vm->log_file) {
                fprintf(vm->log_file, "Module %s: executed %lu instructions\n",
                        mod->name, (unsigned long)instr_count);
            }
        }

        /* Завершение цикла: гарантируем минимум cycle_time_ms (если не tick_timing) */
        if (!vm->config.enable_tick_timing) {
            uint64_t expected_end = cycle_start_ms + (uint64_t)vm->config.cycle_time_ms;
            if (vm->time_ms < expected_end) vm->time_ms = expected_end;
        }

        update_all_timers(vm); /* предполагается адаптировать под per-vm, при необходимости заменить на update_all_timers(vm) */

        uint32_t end_hash = calculate_registers_hash(vm->reg, REG_COUNT);

        if (vm->config.enable_hash_check) {
            if (vm->cycle_count > 1 && end_hash != vm->prev_cycle_hash) {
                if (vm->log_file) fprintf(vm->log_file, "!!! HASH MISMATCH: Previous 0x%08X, current 0x%08X\n",
                                     vm->prev_cycle_hash, end_hash);
                printf("!!! HASH MISMATCH detected on cycle %u\n", vm->cycle_count);
            }
            uint32_t cur_prog_hash = calculate_memory_hash(vm->mem, vm->PC_START, vm->program_size);
            if (cur_prog_hash != vm->program_hash) {
                if (vm->log_file) fprintf(vm->log_file, "!!! PROGRAM MEMORY HASH MISMATCH\n");
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
        long remaining_ms = vm->config.cycle_time_ms - elapsed_ms;

        if (vm->logging_enabled && vm->log_file) {
            fprintf(vm->log_file, "VM elapsed this cycle: %ld ms\n", elapsed_ms);
            fprintf(vm->log_file, "=== CYCLE %u END: %lu instructions in %ld ms ===\n",
                    vm->cycle_count, (unsigned long)total_instr_count, elapsed_ms);
        }

        printf("Cycle %u: %lu instructions, %ld ms elapsed, time_ms=%llu\n",
               vm->cycle_count, (unsigned long)total_instr_count, elapsed_ms,
               (unsigned long long)vm->time_ms);

        if (remaining_ms > 0) {
            struct timespec sleep_time;
            sleep_time.tv_sec = remaining_ms / 1000;
            sleep_time.tv_nsec = (remaining_ms % 1000) * 1000000L;
            nanosleep(&sleep_time, NULL);
        } 
        else if (vm->config.enable_cycle_check) {
            printf("!!! WARNING: Cycle %u overrun by %ld ms\n", vm->cycle_count, -remaining_ms);
        }
            

    }

    close_logging(vm);
    return 0;
}

/* ----------------------------
   main — CLI (адаптирован)
   ---------------------------- */
#ifndef UNIT_TEST
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <program1.bin> [program2.bin ...] [--raw-halt]\n", argv[0]);
        return 1;
    }

    int raw_halt = 0;
    int file_args_end = argc;

    if (argc >= 2 && strcmp(argv[argc-1], "--raw-halt") == 0) {
        raw_halt = 1;
        file_args_end = argc - 1;
    }

    int file_count = file_args_end - 1;
    if (file_count < 1) {
        fprintf(stderr, "Error: no program files specified\n");
        return 1;
    }

    const char **filenames = (const char **)malloc(sizeof(char*) * file_count);
    if (!filenames) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < file_count; ++i) filenames[i] = argv[1 + i];

    vm_state_t *vm = vm_create();
    if (!vm) {
        fprintf(stderr, "Failed to create VM\n");
        free(filenames);
        return 1;
    }

    /* Установим глобальную переменную для обработчика сигналов (CLI: один активный VM) */
    g_active_vm_for_signal = vm;
    signal(SIGINT, handle_sigterm_global);
    signal(SIGTERM, handle_sigterm_global);

    /* timers / tables — ожидается адаптация реализаций к vm аргументу */
    timers_init(vm); /* если ваша реализация требует vm, замените на timers_init(vm) */
    vm_tables_init(vm);

    if (load_programs(vm, filenames, file_count) != 0) {
        fprintf(stderr, "Failed to load programs\n");
        vm_tables_destroy(vm);
        vm_destroy(vm);
        free(filenames);
        return 1;
    }

    run_program(vm);

    vm_tables_destroy(vm);
    vm_destroy(vm);
    free(filenames);

    return 0;
}
#endif
