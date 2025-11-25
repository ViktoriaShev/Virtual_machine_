#include "vm32.h"
#include "funcs.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>


// старт PC
uint32_t PC_START = 0x3000;

// память (лучше выделять динамически)
uint32_t *mem = NULL;

// регистры
uint32_t reg[REG_COUNT] = {0};

// таблица инструкций
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

    op_limit, op_sel, op_mux
};

bool running = true;

// Флаги состояния
bool zero_flag = false;
bool compare_flag = false;

// Стек для PC (например, для CALL/RETURN или FOR/NEXT)
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

// Выполнение одной инструкции
uint32_t execute_instruction(uint32_t pc) {
    uint32_t instr = mem[pc];

    uint8_t opcode = OPC(instr);  // старшие 7 бит
    if (opcode >= OPCODE_COUNT) {
        printf("Unknown opcode %u at PC=0x%X\n", opcode, pc);
        running = false;
        return pc + 1;
    }

    // Вызов функции из таблицы op_ex
    op_ex[opcode](instr);

    // Обновление флагов (например, для арифметики)
    uint8_t reg_a = (instr >> 17) & 0xFF;
    zero_flag = (reg[reg_a] == 0);
    compare_flag = (reg[reg_a] > 0);

    return pc + 1; // следующая инструкция
}

// Основной цикл выполнения программы
void run_program() {
    uint32_t pc = PC_START;

    while (running) {
        if (pc >= UINT32_MAX) {
            printf("PC out of range\n");
            break;
        }
        pc = execute_instruction(pc);

        // Если нужна имитация цикла по тактам
        usleep(1000);  // 1 мс
    }
}

// Точка входа
int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <program.bin>\n", argv[0]);
        return 1;
    }

    mem = (uint32_t*)calloc(MEM_SIZE, sizeof(uint32_t));
    if (!mem) {
        printf("Memory allocation failed\n");
        return 1;
    }

    // Инициализация регистров
    for (int i = 0; i < REG_COUNT; i++) reg[i] = 0;

    // Загрузка программы (например, бинарного файла)
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        printf("Failed to open program file %s\n", argv[1]);
        free(mem);
        return 1;
    }
    size_t read_count = fread(mem + PC_START, sizeof(uint32_t), MEM_SIZE - PC_START, fp);

    printf("Loaded %zu instructions\n", read_count);
    fclose(fp);

    // Запуск VM
    run_program();

    free(mem);
    return 0;
}
