#define _POSIX_C_SOURCE 199309L
#include "vm32.h"
#include "funcs.h"
#include "debug.h"

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// Конфигурация

vm_config_t vm_config = {
    .clock_rate_hz = 0,        // 0 = без ограничения
    .cycle_time_ms = 100,      // 100 мс цикл (типично для ПЛК)
    .enable_cycle_check = true,  // Отключена проверка перерасхода
    .enable_hash_check = true    // Отключена проверка хеша
};

uint32_t PC_START = 0x3000;
#define MAX_INSTRUCTIONS 100000  // защита от бесконечных циклов

// Глобальные переменные VM
uint32_t PC = 0;
uint8_t *mem = NULL;
uint32_t reg[REG_COUNT] = {0};
bool running = true;

// Таблица инструкций
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
    op_halt, // добавляем HALT
    op_nop, op_nop, op_nop, op_nop, op_nop, // заполнители
    // остальные слоты NULL или op_nop
};

// Стек для PC
#define PC_STACK_SIZE 256
uint32_t pc_stack[PC_STACK_SIZE];
uint32_t pc_stack_ptr = 0;

static inline void push_pc(uint32_t pc) {
    if (pc_stack_ptr < PC_STACK_SIZE) pc_stack[pc_stack_ptr++] = pc;
}

static inline uint32_t pop_pc() {
    if (pc_stack_ptr == 0) return 0;
    return pc_stack[--pc_stack_ptr];
}

// Основной цикл выполнения
void run_program() {
    struct timespec cycle_start, cycle_end;
    init_logging();
    reg[0] = 10;
    reg[1] = 5;

    PC = PC_START;
    running = true;
    
    if (logging_enabled && log_file) {
        fprintf(log_file, "Program execution started at PC=0x%04X\n", PC);
        fprintf(log_file, "Maximum instructions: %d\n\n", MAX_INSTRUCTIONS);
    }
    
    printf("Starting VM execution at PC=0x%04X\n", PC);
    
    uint64_t instr_count = 0;
    
    while (running && instr_count < MAX_INSTRUCTIONS) {
        clock_gettime(CLOCK_MONOTONIC, &cycle_start);
        // Проверка границ
        if (PC >= MEM_BYTES - 3) {
            printf("PC out of memory bounds (pc=0x%X)\n", PC);
            if (logging_enabled && log_file) {
                fprintf(log_file, "ERROR: PC out of bounds (0x%X)\n", PC);
            }
            break;
        }
        
        uint32_t instr = mr32(PC);
        
        // Проверка на пустую память (все нули = конец программы)
        if (instr == 0 && PC > PC_START) {
            printf("Reached end of program (all zeros at PC=0x%X)\n", PC);
            if (logging_enabled && log_file) {
                fprintf(log_file, "INFO: End of program reached\n");
            }
            break;
        }
        
        uint8_t opcode = OPC(instr);
        
        if (opcode >= OPCODE_COUNT || op_ex[opcode] == NULL) {
            printf("Invalid opcode %u at PC=0x%X\n", opcode, PC);
            if (logging_enabled && log_file) {
                fprintf(log_file, "ERROR: Invalid opcode %u at PC=0x%X\n", opcode, PC);
            }
            break;
        }
        
        uint32_t old_pc = PC;
        
        // Логируем ДО выполнения
        log_before(PC, instr);
        
        // Выполняем инструкцию
        op_ex[opcode](instr);
        
        // Если PC не изменился, переходим к следующей инструкции
        if (PC == old_pc) {
            PC += 4;
        }
        
        // Логируем ПОСЛЕ выполнения
        log_after(PC);
        
        instr_count++;
        
        // Периодический вывод прогресса
        if (instr_count % 1000 == 0) {
            printf("  Executed %lu instructions...\n", (unsigned long)instr_count);

        
        }

        // Ждем до конца цикла
        clock_gettime(CLOCK_MONOTONIC, &cycle_end);
        long elapsed_ms = (cycle_end.tv_sec - cycle_start.tv_sec) * 1000 +
                          (cycle_end.tv_nsec - cycle_start.tv_nsec) / 1000000;
        
        long remaining_ms = vm_config.cycle_time_ms - elapsed_ms;
        
        if (remaining_ms > 0) {
            usleep(remaining_ms * 1000);
        } else if (vm_config.enable_cycle_check) {
            fprintf(stderr, "WARNING: Cycle overrun by %ld ms\n", -remaining_ms);
        }
    }
    
    if (instr_count >= MAX_INSTRUCTIONS) {
        printf("WARNING: Instruction limit reached (%d instructions)\n", MAX_INSTRUCTIONS);
        if (logging_enabled && log_file) {
            fprintf(log_file, "WARNING: Maximum instruction limit reached\n");
        }
    }
    
    printf("VM stopped. Total instructions: %lu, Final PC: 0x%X\n", 
           (unsigned long)instr_count, PC);
    
    close_logging();
}

// Загрузка бинарника в память
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
    
    // Выводим первые несколько инструкций
    printf("First instructions:\n");
    for (size_t i = 0; i < 3 && (i * 4) < bytes_read; i++) {
        uint32_t instr = mr32(PC_START + i * 4);
        printf("  [0x%04X] 0x%08X - %s\n",
       (unsigned int)(PC_START + i * 4), instr, opcode_name(OPC(instr)));

    }
    
    fclose(fp);
}

// Точка входа
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <program.bin>\n", argv[0]);
        return 1;
    }
    
    mem = (uint8_t*)calloc(1, MEM_BYTES);
    if (!mem) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    for (int i = 0; i < REG_COUNT; i++) reg[i] = 0;
    
    load_program(argv[1]);
    run_program();
    
    free(mem);
    return 0;
}