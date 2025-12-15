#define _POSIX_C_SOURCE 200809L  // можно и без этого, если не требуется

#include "vm32.h"
#include "funcs.h"
#include "debug.h"
#include "hashing.h"
#include "cleanup.h"
#include "timers.h"

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

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

/* ----------------------------
   Основной цикл выполнения
   ---------------------------- */
void run_program(void) {
    struct timespec cycle_start, cycle_end;
    
    init_timer();
    init_logging();

     // ⚠️ Устанавливаем CRC32 для production
    vm_config.hash_algo = HASH_CRC32;
    vm_config.enable_hash_check = true;
    
    printf("Starting VM execution with cycle-based execution\n");
    printf("Cycle time: %u ms\n", vm_config.cycle_time_ms);
    printf("Hash check: %s\n", vm_config.enable_hash_check ? "enabled" : "disabled");
    printf("Cycle overrun check: %s\n", vm_config.enable_cycle_check ? "enabled" : "disabled");
    printf("Tick timing: %s\n", vm_config.enable_tick_timing ? "enabled" : "disabled");
    printf("\n");
    
    /* Основной цикл выполнения */
    while (1) {
        cycle_count++;
        clock_gettime(CLOCK_MONOTONIC, &cycle_start);
        
        // Сброс состояния для нового цикла
        running = true;
        PC = PC_START;
        pc_stack_ptr = 0;

        if (atomic_load(&vm_stop_requested)) {

            /* If exit() already set exit_code — leave it */
            if (vm_exit_code == 0) {
                vm_exit_code = 0; // normal shutdown
            }

            run_cleanups();
            break;
        }

        if (logging_enabled && log_file) {
            fprintf(log_file, "\n=== CYCLE %u START ===\n", cycle_count);
        }
        
        // Вычисляем хеш состояния регистров перед выполнением цикла
        uint32_t start_hash = calculate_registers_hash(reg, REG_COUNT);

        if (logging_enabled && log_file) {
            fprintf(log_file, "Registers hash at start: 0x%08X\n", start_hash);
        }

        // если это первый цикл — и включена проверка, и prev_cycle_hash==0 — инициализируем prev_cycle_hash
        if (vm_config.enable_hash_check && cycle_count == 1) {
            prev_cycle_hash = start_hash;
            if (logging_enabled && log_file) {
                fprintf(log_file, "Initialized prev_cycle_hash = 0x%08X\n", prev_cycle_hash);
            }
        }
        
        /* Выполнение программы до завершения или остановки */
        uint64_t instr_count = 0;
        
        while (running && instr_count < MAX_INSTRUCTIONS) {
            // Проверка границ памяти
            if (PC >= MEM_BYTES - 3) {
                printf("Cycle %u: PC out of memory bounds (0x%X)\n", cycle_count, PC);
                if (logging_enabled && log_file) {
                    fprintf(log_file, "ERROR: PC out of bounds (0x%X)\n", PC);
                }
                running = false;
                break;
            }
            
            uint32_t instr = mr32(PC);
            
            // Проверка на пустую память (конец программы)
            if (instr == 0 && PC > PC_START) {
                if (logging_enabled && log_file) {
                    fprintf(log_file, "INFO: End of program reached at PC=0x%X\n", PC);
                }
                running = false;
                break;
            }
            
            /* Получаем/декодируем инструкцию через кеш */
            decoded_instr_t *d = decode_instruction(PC);
            if (!d) {
                printf("Cycle %u: Failed to decode instruction at PC=0x%X\n", cycle_count, PC);
                if (logging_enabled && log_file) {
                    fprintf(log_file, "ERROR: Failed to decode instruction at PC=0x%X\n", PC);
                }
                running = false;
                break;
            }

            uint8_t opcode = OPC(instr);
            
            // Проверка валидности опкода
            if (opcode >= OPCODE_COUNT || op_ex[opcode] == NULL) {
                printf("Cycle %u: Invalid opcode %u at PC=0x%X\n", cycle_count, opcode, PC);
                if (logging_enabled && log_file) {
                    fprintf(log_file, "ERROR: Invalid opcode %u at PC=0x%X\n", opcode, PC);
                }
                running = false;
                break;
            }
            
            uint32_t old_pc = PC;
            
            // Логируем ДО выполнения
            log_before(PC, instr);
            
            /* Выполняем инструкцию — вызываем обработчик с raw_instr */
            op_ex[opcode](d->raw_instr);
            
            // Если PC не изменился, переходим к следующей инструкции
            if (PC == old_pc) {
                PC += 4;
            }
            
            // Логируем ПОСЛЕ выполнения
            log_after(PC);
            
            instr_count++;
            
            // Ожидание такта (если включено)
            wait_for_tick();
            update_all_timers();
        }
        
        // Вычисляем хеш состояния регистров после выполнения цикла
        uint32_t end_hash = calculate_registers_hash(reg, REG_COUNT);

        if (vm_config.enable_hash_check) {
            if (cycle_count > 1 && end_hash != prev_cycle_hash) {
                fprintf(log_file, "!!! HASH MISMATCH: Previous end hash was 0x%08X, current end hash 0x%08X\n",
                        prev_cycle_hash, end_hash);
                printf("!!! HASH MISMATCH detected on cycle %u\n", cycle_count);
            }

            /* Проверяем целостность образа в памяти */
            uint32_t cur_prog_hash = calculate_memory_hash(mem, PC_START, program_size);
            if (cur_prog_hash != program_hash) {
                fprintf(log_file, "!!! PROGRAM MEMORY HASH MISMATCH: expected 0x%08X got 0x%08X\n",
                        program_hash, cur_prog_hash);
                printf("!!! PROGRAM MEMORY HASH MISMATCH on cycle %u\n", cycle_count);
            }
        }
        prev_cycle_hash = end_hash;

        if (logging_enabled && log_file) {
            fprintf(log_file, "Registers hash at end:   0x%08X\n", end_hash);
        }


        
        // Измерение времени выполнения цикла
        clock_gettime(CLOCK_MONOTONIC, &cycle_end);
        long elapsed_ms = get_elapsed_ms(cycle_start, cycle_end);
        long remaining_ms = vm_config.cycle_time_ms - elapsed_ms;
        
        if (logging_enabled && log_file) {
            fprintf(log_file, "=== CYCLE %u END: executed %lu instructions in %ld ms ===\n",
                    cycle_count, (unsigned long)instr_count, elapsed_ms);
        }
        
        printf("Cycle %u: %lu instructions, %ld ms elapsed\n", 
               cycle_count, (unsigned long)instr_count, elapsed_ms);
        
        // Если цикл выполнился быстрее заданного времени - ждем
        if (remaining_ms > 0) {
            if (logging_enabled && log_file) {
                fprintf(log_file, "  Waiting for %ld ms to complete cycle\n", remaining_ms);
            }
            usleep(remaining_ms * 1000);
        } else if (vm_config.enable_cycle_check) {
            printf("!!! WARNING: Cycle %u overrun by %ld ms\n", cycle_count, -remaining_ms);
            if (logging_enabled && log_file) {
                fprintf(log_file, "!!! WARNING: Cycle overrun by %ld ms\n", -remaining_ms);
            }
        }
        
        // Логирование общего времени цикла
        if (logging_enabled && log_file) {
            clock_gettime(CLOCK_MONOTONIC, &cycle_end);
            long total_cycle_ms = get_elapsed_ms(cycle_start, cycle_end);
            fprintf(log_file, "  Total cycle time: %ld ms\n", total_cycle_ms);
        }
        
        // Проверка на достижение лимита инструкций
        if (instr_count >= MAX_INSTRUCTIONS) {
            printf("WARNING: Instruction limit reached in cycle %u\n", cycle_count);
            if (logging_enabled && log_file) {
                fprintf(log_file, "WARNING: Maximum instruction limit reached\n");
            }
        }
    }
    
    close_logging();
}

/* ----------------------------
   Загрузка бинарника в память
   ---------------------------- */
void load_program(const char *fname) {
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open program file %s\n", fname);
        exit(1);
    }
    
    // Узнаем размер файла
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    printf("File size: %ld bytes\n", fsize);
    fseek(fp, 0, SEEK_SET);
    
    size_t bytes_read = fread(mem + PC_START, 1, MEM_BYTES - PC_START, fp);
    printf("Loaded %zu bytes into memory at 0x%X\n", bytes_read, PC_START);
    program_size = bytes_read;
    program_hash = calculate_memory_hash(mem, PC_START, program_size);
    printf("Loaded %zu bytes into memory at 0x%X\n", bytes_read, PC_START);
    printf("Program hash (CRC32): 0x%08X\n", program_hash);

    // Выводим первые несколько инструкций
    printf("First instructions:\n");
    for (size_t i = 0; i < 3 && (i * 4) < bytes_read; i++) {
        uint32_t instr = mr32(PC_START + i * 4);
        printf("  [0x%04X] 0x%08X - %s\n",
               (unsigned int)(PC_START + i * 4), instr, opcode_name(OPC(instr)));
    }
    
    fclose(fp);
}

/* ----------------------------
   Точка входа
   ---------------------------- */
int main(int argc, char **argv) {

    signal(SIGINT, handle_sigterm);
    signal(SIGTERM, handle_sigterm);
    if (argc != 2) {
        printf("Usage: %s <program.bin>\n", argv[0]);
        return 1;
    }
    
    mem = (uint8_t*)calloc(1, MEM_BYTES);
    if (!mem) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    for (int i = 0; i < REG_COUNT; i++) {
        reg[i] = 0;
    }
    
    reg[0] = 5;
    reg[1] = 3;

    timers_init();

    /* Инициализация таблиц хеширования/кеша */
    vm_tables_init();

    load_program(argv[1]);
    run_program();
    
    /* Очистка таблиц и ресурсов */
    vm_tables_destroy();
    free(mem);
    return vm_exit_code;
}