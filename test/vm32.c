// vm32.c
#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "funcs.h"
#include "debug.h"
#include "hashing.h"
#include "cleanup.h"
#include "timers.h"
#include "vm_tables.h"   // теперь таблицы вынесены сюда
#include "loader.h"

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>   // usleep

/* ----------------------------
   Конфигурация VM
   ---------------------------- */
vm_config_t vm_config = {
    .clock_rate_hz = 100,
    .cycle_time_ms = 1000,
    .enable_cycle_check = true,
    .enable_hash_check = true,
    .enable_tick_timing = false,
    .hash_algo = HASH_CRC32
};

/* ----------------------------
   Глобальные переменные VM
   (определения — внешний интерфейс в vm32.h)
   ---------------------------- */
uint32_t PC_START = 0x3000;
#define MAX_INSTRUCTIONS 100000

uint32_t PC = 0;
uint8_t *mem = NULL;
uint32_t reg[REG_COUNT] = {0};
bool running = true;
uint64_t time_ms = 0;
static uint64_t instr_ns_accum = 0;

/* Program integrity */
uint32_t program_hash = 0;
size_t program_size = 0;

struct timespec last_tick_time = {0, 0};
uint32_t cycle_count = 0;
uint32_t prev_cycle_hash = 0;

/* Модули (список загруженных бинарников) */
module_info_t *modules = NULL;
size_t module_count = 0;

/* Таблица опкодов — остаётся в vm32.c (вызываемые функции в funcs.c) */
op_ex_f op_ex[OPCODE_COUNT] = {
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
    op_jmp, op_jmp_if, op_jmp_if_not,
    op_exit,
    op_halt,
    op_nop,
};

/* ----------------------------
   Вспомогательные функции времени
   ---------------------------- */
void init_timer(void) {
    clock_gettime(CLOCK_MONOTONIC, &last_tick_time);
}

long get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 +
           (end.tv_nsec - start.tv_nsec) / 1000000;
}

void wait_for_tick(void) {
    if (!vm_config.enable_tick_timing || vm_config.clock_rate_hz == 0) {
        return;
    }
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    long interval_ns = 1000000000 / vm_config.clock_rate_hz;
    long elapsed_ns = (current_time.tv_sec - last_tick_time.tv_sec) * 1000000000 +
                      (current_time.tv_nsec - last_tick_time.tv_nsec);

    if (elapsed_ns < interval_ns) {
        long remaining_ns = interval_ns - elapsed_ns;
        struct timespec sleep_time = {0, remaining_ns};
        nanosleep(&sleep_time, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &last_tick_time);
}

/* ----------------------------
   PC stack helpers
   ---------------------------- */
#define PC_STACK_SIZE 256
uint32_t pc_stack[PC_STACK_SIZE];
uint32_t pc_stack_ptr = 0;

static inline void push_pc(uint32_t pc) {
    if (pc_stack_ptr < PC_STACK_SIZE) pc_stack[pc_stack_ptr++] = pc;
}

static inline uint32_t pop_pc(void) {
    if (pc_stack_ptr == 0) return 0;
    return pc_stack[--pc_stack_ptr];
}

void handle_sigterm(int sig) {
    (void)sig;
    atomic_store(&vm_stop_requested, true);
}

/* ----------------------------
   Основной цикл выполнения
   (не трогаем логику — она остаётся прежней, но зависимости вынесены)
   ---------------------------- */
void run_program(void) {
    struct timespec cycle_start, cycle_end;

    init_timer();
    init_logging();

    struct timespec __now;
    clock_gettime(CLOCK_MONOTONIC, &__now);
    time_ms = (uint64_t)__now.tv_sec * 1000ULL + (uint64_t)(__now.tv_nsec / 1000000ULL);

    vm_config.hash_algo = HASH_CRC32;
    vm_config.enable_hash_check = true;

    printf("Starting VM execution with cycle-based execution\n");
    printf("Cycle time: %u ms\n", vm_config.cycle_time_ms);
    printf("Hash check: %s\n", vm_config.enable_hash_check ? "enabled" : "disabled");
    printf("Cycle overrun check: %s\n", vm_config.enable_cycle_check ? "enabled" : "disabled");
    printf("Tick timing: %s\n", vm_config.enable_tick_timing ? "enabled" : "disabled");
    printf("Clock rate: %u Hz\n", vm_config.clock_rate_hz);
    printf("\n");

    while (1) {
        cycle_count++;
        clock_gettime(CLOCK_MONOTONIC, &cycle_start);
        if (atomic_load(&vm_stop_requested)) {
            run_cleanups();
            break;
        }
        uint64_t cycle_start_ms = time_ms;
        if (logging_enabled && log_file) {
            fprintf(log_file, "\n=== CYCLE %u START (time_ms=%llu) ===\n", cycle_count, (unsigned long long)time_ms);
        }

        uint32_t start_hash = calculate_registers_hash(reg, REG_COUNT);
        if (logging_enabled && log_file) {
            fprintf(log_file, "Registers hash at start: 0x%08X\n", start_hash);
        }

        if (vm_config.enable_hash_check && cycle_count == 1) {
            prev_cycle_hash = start_hash;
            if (logging_enabled && log_file) {
                fprintf(log_file, "Initialized prev_cycle_hash = 0x%08X\n", prev_cycle_hash);
            }
        }

        uint64_t total_instr_count = 0;

        for (size_t mod_idx = 0; mod_idx < module_count; ++mod_idx) {
            module_info_t *mod = &modules[mod_idx];

            if (logging_enabled && log_file) {
                fprintf(log_file, "\n--- Executing module: %s (0x%X) ---\n", mod->name, mod->addr);
            }

            running = true;
            PC = mod->addr;
            pc_stack_ptr = 0;

            uint64_t instr_count = 0;
            uint32_t module_end = mod->addr + mod->size;

            while (running && instr_count < MAX_INSTRUCTIONS) {
                if (PC >= module_end || PC < mod->addr) {
                    if (logging_enabled && log_file) {
                        fprintf(log_file, "INFO: Module %s completed (PC=0x%X)\n", mod->name, PC);
                    }
                    break;
                }

                uint32_t instr = mr32(PC);
                if (instr == 0) {
                    if (logging_enabled && log_file) {
                        fprintf(log_file, "INFO: End of module %s at PC=0x%X\n", mod->name, PC);
                    }
                    break;
                }

                decoded_instr_t *d = vm_decode_instruction(PC);
                if (!d) {
                    printf("Cycle %u, Module %s: Failed to decode at PC=0x%X\n",
                           cycle_count, mod->name, PC);
                    running = false;
                    break;
                }

                uint8_t opcode = OPC(instr);
                if (opcode >= OPCODE_COUNT || op_ex[opcode] == NULL) {
                    printf("Cycle %u, Module %s: Invalid opcode %u at PC=0x%X\n",
                           cycle_count, mod->name, opcode, PC);
                    running = false;
                    break;
                }

                uint32_t old_pc = PC;
                log_before(PC, instr);
                op_ex[opcode](d->raw_instr);
                if (PC == old_pc) PC += 4;
                log_after(PC);

                instr_count++;
                wait_for_tick();

                if (vm_config.enable_tick_timing && vm_config.clock_rate_hz != 0) {
                    struct timespec __now;
                    clock_gettime(CLOCK_MONOTONIC, &__now);
                    time_ms = (uint64_t)__now.tv_sec * 1000ULL + (uint64_t)(__now.tv_nsec / 1000000ULL);
                } else if (vm_config.clock_rate_hz != 0) {
                    uint64_t instr_ns = 1000000000ULL / (uint64_t)vm_config.clock_rate_hz;
                    instr_ns_accum += instr_ns;
                    uint64_t add_ms = instr_ns_accum / 1000000ULL;
                    if (add_ms) {
                        time_ms += add_ms;
                        instr_ns_accum -= add_ms * 1000000ULL;
                    }
                }
            }

            total_instr_count += instr_count;

            if (logging_enabled && log_file) {
                fprintf(log_file, "Module %s: executed %lu instructions\n",
                        mod->name, (unsigned long)instr_count);
            }
        }

        /* Завершение цикла: гарантируем минимум cycle_time_ms (если не tick_timing) */
        if (!vm_config.enable_tick_timing) {
            uint64_t expected_end = cycle_start_ms + (uint64_t)vm_config.cycle_time_ms;
            if (time_ms < expected_end) time_ms = expected_end;
        }

        update_all_timers();

        uint32_t end_hash = calculate_registers_hash(reg, REG_COUNT);

        if (vm_config.enable_hash_check) {
            if (cycle_count > 1 && end_hash != prev_cycle_hash) {
                fprintf(log_file, "!!! HASH MISMATCH: Previous 0x%08X, current 0x%08X\n",
                        prev_cycle_hash, end_hash);
                printf("!!! HASH MISMATCH detected on cycle %u\n", cycle_count);
            }
            uint32_t cur_prog_hash = calculate_memory_hash(mem, PC_START, program_size);
            if (cur_prog_hash != program_hash) {
                fprintf(log_file, "!!! PROGRAM MEMORY HASH MISMATCH\n");
                printf("!!! PROGRAM MEMORY HASH MISMATCH on cycle %u\n", cycle_count);
            }
        }
        prev_cycle_hash = end_hash;

        if (logging_enabled && log_file) {
            fprintf(log_file, "Registers hash at end: 0x%08X\n", end_hash);
            fprintf(log_file, "time_ms at end: %llu (delta: %llu ms)\n",
                    (unsigned long long)time_ms,
                    (unsigned long long)(time_ms - cycle_start_ms));
        }

        clock_gettime(CLOCK_MONOTONIC, &cycle_end);
        long elapsed_ms = get_elapsed_ms(cycle_start, cycle_end);
        long remaining_ms = vm_config.cycle_time_ms - elapsed_ms;

        if (logging_enabled && log_file) {
            fprintf(log_file, "VM elapsed this cycle: %ld ms\n", elapsed_ms);
            fprintf(log_file, "=== CYCLE %u END: %lu instructions in %ld ms ===\n",
                    cycle_count, (unsigned long)total_instr_count, elapsed_ms);
        }

        printf("Cycle %u: %lu instructions, %ld ms elapsed, time_ms=%llu\n",
               cycle_count, (unsigned long)total_instr_count, elapsed_ms,
               (unsigned long long)time_ms);

        if (remaining_ms > 0) {
            usleep((useconds_t)(remaining_ms * 1000));
        } else if (vm_config.enable_cycle_check) {
            printf("!!! WARNING: Cycle %u overrun by %ld ms\n", cycle_count, -remaining_ms);
        }
    }

    close_logging();
}


#ifndef UNIT_TEST
int main(int argc, char **argv) {
    signal(SIGINT, handle_sigterm);
    signal(SIGTERM, handle_sigterm);

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

    for (int i = 0; i < file_count; ++i) {
        filenames[i] = argv[1 + i];
    }

    mem = (uint8_t*)calloc(1, MEM_BYTES);
    if (!mem) {
        printf("Memory allocation failed\n");
        free(filenames);
        return 1;
    }

    for (int i = 0; i < REG_COUNT; i++) reg[i] = 0;

    reg[0] = 5;
    reg[1] = 3;

    timers_init();
    vm_tables_init();

    load_programs(filenames, file_count);

    run_program();

    vm_tables_destroy();
    free(mem);
    free(filenames);

    for (size_t i = 0; i < module_count; ++i) free(modules[i].name);
    free(modules);

    return vm_exit_code;
}
#endif
