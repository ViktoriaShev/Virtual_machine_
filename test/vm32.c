#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "funcs.h"
#include "debug.h"
#include "hashing.h"
#include "cleanup.h"
#include "timers.h"
#include "vm_tables.h"

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
    .clock_rate_hz = 100,        // 100 инструкций/сек (0 = без ограничения)
    .cycle_time_ms = 1000,       // 1000 мс = 1 секунда на цикл
    .enable_cycle_check = true,  // Включена проверка перерасхода
    .enable_hash_check = true,   // Включена проверка хеша
    .enable_tick_timing = false , // Отключено (можно включить для медленного выполнения)
    .hash_algo = HASH_CRC32      // По умолчанию — CRC32 (можно сменить на HASH_FNV1A)
};

/* ----------------------------
   Глобальные переменные VM
   ---------------------------- */
uint32_t PC_START = 0x3000;
#define MAX_INSTRUCTIONS 100000  // Защита от бесконечных циклов

uint32_t PC = 0;
uint8_t *mem = NULL;
uint32_t reg[REG_COUNT] = {0};
bool running = true;
uint64_t time_ms = 0;
static uint64_t instr_ns_accum = 0;
/* Переменные для работы с циклами */

/* Program integrity */
static uint32_t program_hash = 0;
static size_t program_size = 0;

struct timespec last_tick_time = {0, 0};
uint32_t cycle_count = 0;
uint32_t prev_cycle_hash = 0;

/* globals */
static hash_table_t *labels = NULL;
static hash_table_t *breakpoints = NULL;
static hash_table_t *decoded_cache = NULL;

module_info_t *modules = NULL;
size_t module_count = 0;
/* ----------------------------
   Таблица инструкций
   ---------------------------- */
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
   Кеш декодированных инструкций
   ---------------------------- */

/* Декодирование с использованием кеша.
 * Если запись есть в decoded_cache по адресу addr — вернём её.
 * Иначе: выделим decoded_instr_t, заполним поля и сохраним в decoded_cache.
 */
static decoded_instr_t *decode_instruction(uint32_t addr) {
    if (!decoded_cache) return NULL;

    uint32_t key = addr;
    decoded_instr_t *cached = (decoded_instr_t *)hash_table_lookup(decoded_cache, &key);
    if (cached) {
        return cached;
    }

    /* Безопасность: убедимся, что адрес в пределах памяти */
    if (addr >= MEM_BYTES - 3) return NULL;

    uint32_t instr = mr32(addr);

    /* Проверка "пустой" инструкции можно оставить вызывающему */
    decoded_instr_t *dec = (decoded_instr_t *)malloc(sizeof(decoded_instr_t));
    if (!dec) {
        fprintf(stderr, "decode_instruction: malloc failed\n");
        return NULL;
    }

    dec->raw_instr = instr;
    dec->opcode = OPC(instr);
    dec->ra = RA(instr);
    dec->rb = RB(instr);
    dec->rc = RC(instr);
    dec->has_immediate = FIMM(instr);
    dec->immediate = dec->has_immediate ? IMM8(instr) : 0;

    /* Сохраняем в кеш — hash_table_insert скопирует ключ (uint32_copy),
       а для значения используется ptr_copy (как задано в vm_tables_init),
       поэтому передаём указатель на структуру. */
    hash_table_insert(decoded_cache, &key, dec);

    return dec;
}

/* --- Test helper: exported wrapper to call static decode_instruction() --- */
decoded_instr_t *vm_decode_instruction(uint32_t addr) {
    return decode_instruction(addr);
}

/* Уничтожение/очистка всех таблиц, вызывается при завершении программы.
   Учитывает, что decoded_cache value_type.del == free, поэтому освобождаются
   и структуры decoded_instr_t. */
void vm_tables_destroy(void) {
    if (labels) {
        hash_table_destroy(labels);
        labels = NULL;
    }
    if (breakpoints) {
        hash_table_destroy(breakpoints);
        breakpoints = NULL;
    }
    if (decoded_cache) {
        hash_table_destroy(decoded_cache);
        decoded_cache = NULL;
    }
    /* Если у вас есть другие таблицы (symbols, string_pool, etc.)
       — тоже вызывать hash_table_destroy для них здесь. */
}


void vm_tables_init(void) {
    labels = hash_table_create(&string_key_type, &uint32_value_type); // name -> addr
    breakpoints = hash_table_create(&uint32_key_type, &uint32_value_type); // addr -> flag
    /* Для кеша — сделаем value_type, который освобождает память при del */
    decoded_cache = hash_table_create(&uint32_key_type, &ptr_value_type);

}

void labels_add(const char *name, uint32_t addr) {
    hash_table_insert(labels, name, &addr); // копируется внутрь таблицы
}

uint32_t *labels_lookup(const char *name) {
    return (uint32_t *)hash_table_lookup(labels, name); // NULL если нет
}

void bp_set(uint32_t addr) {
    uint32_t v = 1;
    hash_table_insert(breakpoints, &addr, &v);
}
void bp_clear(uint32_t addr) {
    hash_table_delete(breakpoints, &addr);
}
bool bp_is_set(uint32_t addr) {
    return hash_table_contains(breakpoints, &addr);
}

/* ----------------------------
   Стек для PC
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
   Утилиты работы с временем
   ---------------------------- */

/* Инициализация системного таймера */
void init_timer(void) {
    clock_gettime(CLOCK_MONOTONIC, &last_tick_time);
}

/* Получение разницы времени в миллисекундах */
long get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 +
           (end.tv_nsec - start.tv_nsec) / 1000000;
}

/* Ожидание следующего такта (если включено ограничение тактовой частоты) */
void wait_for_tick(void) {
    if (!vm_config.enable_tick_timing || vm_config.clock_rate_hz == 0) {
        return;
    }
    
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    // Интервал между тактами в наносекундах
    long interval_ns = 1000000000 / vm_config.clock_rate_hz;
    
    // Прошедшее время с последнего такта
    long elapsed_ns = (current_time.tv_sec - last_tick_time.tv_sec) * 1000000000 +
                      (current_time.tv_nsec - last_tick_time.tv_nsec);
    
    // Если до следующего такта осталось время - ждем
    if (elapsed_ns < interval_ns) {
        long remaining_ns = interval_ns - elapsed_ns;
        struct timespec sleep_time = {0, remaining_ns};
        nanosleep(&sleep_time, NULL);
    }
    
    // Фиксируем время начала нового такта
    clock_gettime(CLOCK_MONOTONIC, &last_tick_time);
}

/*bool module_needs_reload(module_info_t *m, const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) return false;
    
    // Сравниваем mtime или размер
    return st.st_size != m->size;
}*/

/* ----------------------------
   Основной цикл выполнения
   ---------------------------- */
void run_program(void) {
    struct timespec cycle_start, cycle_end;
    
    init_timer();
    init_logging();

    struct timespec __now;
    clock_gettime(CLOCK_MONOTONIC, &__now);
    time_ms = (uint64_t)__now.tv_sec * 1000ULL + (uint64_t)(__now.tv_nsec / 1000000ULL);
    
     // ⚠️ Устанавливаем CRC32 для production
    vm_config.hash_algo = HASH_CRC32;
    vm_config.enable_hash_check = true;
    
    printf("Starting VM execution with cycle-based execution\n");
    printf("Cycle time: %u ms\n", vm_config.cycle_time_ms);
    printf("Hash check: %s\n", vm_config.enable_hash_check ? "enabled" : "disabled");
    printf("Cycle overrun check: %s\n", vm_config.enable_cycle_check ? "enabled" : "disabled");
    printf("Tick timing: %s\n", vm_config.enable_tick_timing ? "enabled" : "disabled");
    printf("Clock rate: %u Hz\n", vm_config.clock_rate_hz);
    printf("\n");
    
    /* Основной цикл выполнения */
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
        
        // Вычисляем хеш состояния регистров перед выполнением цикла
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
        
        /* Выполняем КАЖДЫЙ модуль в этом цикле */
        uint64_t total_instr_count = 0;
        
        for (size_t mod_idx = 0; mod_idx < module_count; ++mod_idx) {
            module_info_t *mod = &modules[mod_idx];
            
            if (logging_enabled && log_file) {
                fprintf(log_file, "\n--- Executing module: %s (0x%X) ---\n", mod->name, mod->addr);
            }
            
            // Сброс состояния для модуля
            running = true;
            PC = mod->addr;
            pc_stack_ptr = 0;
            
            uint64_t instr_count = 0;
            uint32_t module_end = mod->addr + mod->size;
            
            while (running && instr_count < MAX_INSTRUCTIONS) {
                // Проверка границ модуля
                if (PC >= module_end || PC < mod->addr) {
                    if (logging_enabled && log_file) {
                        fprintf(log_file, "INFO: Module %s completed (PC=0x%X)\n", mod->name, PC);
                    }
                    break;
                }
                
                uint32_t instr = mr32(PC);
                
                // Проверка на пустую память
                if (instr == 0) {
                    if (logging_enabled && log_file) {
                        fprintf(log_file, "INFO: End of module %s at PC=0x%X\n", mod->name, PC);
                    }
                    break;
                }
                
                decoded_instr_t *d = decode_instruction(PC);
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
                
                if (PC == old_pc) {
                    PC += 4;
                }
                
                log_after(PC);
                
                instr_count++;
                wait_for_tick();

                if (vm_config.enable_tick_timing && vm_config.clock_rate_hz != 0) {
                    struct timespec __now;
                    clock_gettime(CLOCK_MONOTONIC, &__now);
                    time_ms = (uint64_t)__now.tv_sec * 1000ULL + (uint64_t)(__now.tv_nsec / 1000000ULL);
                } else if (vm_config.clock_rate_hz != 0) {
                    /* Более точное симулирование времени на инструкцию:
                    считаем наносекунды на инструкцию и аккумулируем остаток. */
                    uint64_t instr_ns = 1000000000ULL / (uint64_t)vm_config.clock_rate_hz;
                    instr_ns_accum += instr_ns;

                    /* Добавляем целые миллисекунды в time_ms, оставшийся ns держим в аккумуляторе */
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
        
        /* === КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ ===
         * Финализация времени в конце цикла:
         * В детерминированном режиме time_ms УЖЕ обновлён через instr_ns_accum
         * в цикле выполнения. Нужно только гарантировать минимум cycle_time_ms.
         */
        if (!vm_config.enable_tick_timing) {
            /* Гарантируем что цикл занял минимум cycle_time_ms */
            uint64_t expected_end = cycle_start_ms + (uint64_t)vm_config.cycle_time_ms;
            if (time_ms < expected_end) {
                time_ms = expected_end;
            }
        }

        /* Теперь обновляем все IEC-таймеры — они смотрят на time_ms */
        update_all_timers();
        
        // Остальная часть (hash check, timing) остается как есть...
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
            usleep(remaining_ms * 1000);
        } else if (vm_config.enable_cycle_check) {
            printf("!!! WARNING: Cycle %u overrun by %ld ms\n", cycle_count, -remaining_ms);
        }
    }
    
    close_logging();
}

void load_programs(const char **fnames, int count) {
    if (count <= 0) {
        fprintf(stderr, "No program files specified\n");
        exit(1);
    }

    // (Re)allocate module table
    modules = (module_info_t *)calloc((size_t)count, sizeof(module_info_t));
    if (!modules) {
        fprintf(stderr, "Failed to allocate modules table\n");
        exit(1);
    }

    uint32_t write_ptr = PC_START;
    program_size = 0;
    program_hash = 0;

    for (int i = 0; i < count; ++i) {
        const char *fname = fnames[i];
        FILE *fp = fopen(fname, "rb");
        if (!fp) {
            fprintf(stderr, "Failed to open program file %s\n", fname);
            exit(1);
        }

        // file size
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (fsize <= 0) {
            fprintf(stderr, "Warning: file %s is empty (size=%ld)\n", fname, fsize);
            fclose(fp);
            // still record a zero-size module (optional)
            modules[module_count].name = strdup(fname);
            modules[module_count].addr = write_ptr;
            modules[module_count].size = 0;
            module_count++;
            continue;
        }

        // Check memory bounds
        if ((uint64_t)write_ptr + (uint64_t)fsize > MEM_BYTES) {
            fprintf(stderr, "Error: loading %s would overflow VM memory (need %ld bytes, available %llu)\n",
                    fname, fsize, (unsigned long long)(MEM_BYTES - write_ptr));
            fclose(fp);
            exit(1);
        }

        size_t bytes_read = fread(mem + write_ptr, 1, (size_t)fsize, fp);
        if (bytes_read != (size_t)fsize) {
            fprintf(stderr, "Error reading file %s: read %zu of %ld\n", fname, bytes_read, fsize);
            fclose(fp);
            exit(1);
        }

        // record module
        modules[module_count].name = strdup(fname);
        modules[module_count].addr = write_ptr;
        modules[module_count].size = (uint32_t)bytes_read;
        module_count++;

        printf("Loaded %zu bytes from %s into memory at 0x%X\n", bytes_read, fname, write_ptr);

        write_ptr += (uint32_t)bytes_read;
        program_size += (uint32_t)bytes_read;

        fclose(fp);
    }

    if (module_count == 0) {
        fprintf(stderr, "ERROR: No modules were loaded successfully\n");
        exit(1);
    }

    printf("\n=== Module Map ===\n");
    for (size_t i = 0; i < module_count; ++i) {
        printf("Module %zu: %s\n", i, modules[i].name);
        printf("  Address: 0x%08X\n", modules[i].addr);
        printf("  Size:    %u bytes\n", modules[i].size);
    }
    printf("==================\n\n");
    
    // compute hash for whole program image
    program_hash = calculate_memory_hash(mem, PC_START, program_size);
    printf("Combined program size: %zu bytes\n", (size_t)program_size);
    printf("Combined program hash (CRC32): 0x%08X\n", program_hash);

    // Print first instructions per module (up to 3 each)
    for (size_t mi = 0; mi < module_count; ++mi) {
        uint32_t base = modules[mi].addr;
        uint32_t sz = modules[mi].size;
        printf("Module %zu: %s at 0x%X (%u bytes)\n", mi, modules[mi].name, base, (unsigned)sz);
        for (size_t i = 0; i < 3 && (i * 4) < sz; ++i) {
            uint32_t instr = mr32(base + (uint32_t)(i * 4));
            printf("  [%s + 0x%04X] 0x%08X - %s\n", modules[mi].name, (unsigned int)(i * 4), instr, opcode_name(OPC(instr)));
        }
    }
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
    
    // Проверяем последний аргумент на --raw-halt
    if (argc >= 2 && strcmp(argv[argc-1], "--raw-halt") == 0) {
        raw_halt = 1;
        file_args_end = argc - 1;
    }
    
    // Подсчитываем количество файлов
    int file_count = file_args_end - 1;  // минус argv[0]
    
    if (file_count < 1) {
        fprintf(stderr, "Error: no program files specified\n");
        return 1;
    }
    
    // Собираем имена файлов
    const char **filenames = (const char **)malloc(sizeof(char*) * file_count);
    if (!filenames) {
        perror("malloc");
        return 1;
    }
    
    for (int i = 0; i < file_count; ++i) {
        filenames[i] = argv[1 + i];
    }
    
    // Выделяем память для VM
    mem = (uint8_t*)calloc(1, MEM_BYTES);
    if (!mem) {
        printf("Memory allocation failed\n");
        free(filenames);
        return 1;
    }
    
    // Инициализация регистров
    for (int i = 0; i < REG_COUNT; i++) {
        reg[i] = 0;
    }
    
    // Тестовые значения
    reg[0] = 5;
    reg[1] = 3;

    timers_init();
    vm_tables_init();

    // Загружаем все программы
    load_programs(filenames, file_count);
    
    // Запускаем VM
    run_program();
    
    // Очистка
    vm_tables_destroy();
    free(mem);
    free(filenames);
    
    // Освобождаем имена модулей
    for (size_t i = 0; i < module_count; ++i) {
        free(modules[i].name);
    }
    free(modules);
    
    return vm_exit_code;
}

#endif 