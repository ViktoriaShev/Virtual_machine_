#define _POSIX_C_SOURCE 199309L
#include "vm32.h"
#include "funcs.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// старт PC (байтовый адрес)
uint32_t PC_START = 0x3000;

// память (байтовая)
uint8_t *mem = NULL;

// регистры
uint32_t reg[REG_COUNT] = {0};

// таблица инструкций (функции должны быть в funcs.c)
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
    // JMP-инструкции
    op_jmp, op_jmp_if, op_jmp_if_not
};

bool running = true;

// Стек для PC (например, для CALL/RETURN)
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
    reg[RPC] = PC_START;

    while (running) {
        if (reg[RPC] + 4 > MEM_BYTES) {
            printf("PC out of range or program end (pc=0x%X)\n", reg[RPC]);
            break;
        }

        uint32_t instr = mr32(reg[RPC]);
        uint8_t opcode = OPC(instr);

        if (opcode >= OPCODE_COUNT) {
            printf("Unknown opcode %u at PC=0x%X\n", opcode, reg[RPC]);
            running = false;
            break;
        }

        uint32_t old_pc = reg[RPC];
        op_ex[opcode](instr);
        if (reg[RPC] == old_pc) reg[RPC] += 4;
    }
}

// Загрузка бинарника в память
void load_program(const char *fname) {
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open program file %s\n", fname);
        exit(1);
    }

    size_t bytes_read = fread(mem + PC_START, 1, MEM_BYTES - PC_START, fp);
    printf("Loaded %zu bytes into memory at 0x%X\n", bytes_read, PC_START);
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