// debug.c
#define _POSIX_C_SOURCE 200809L

// debug.c (updated)
// Detailed VM execution logger with correct immediate (FIMM) handling.

#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "debug.h"
#include "funcs.h"

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

/* Ensure constants exist */
#ifndef MEM_LOG_SIZE
#define MEM_LOG_SIZE 256
#endif

/* --- Provide globals (definitions) to satisfy linker and to be used by vm32.c --- */
bool logging_enabled = false;
bool verbose_logging = false;
FILE *log_file = NULL;

/* Таблица имен опкодов — синхронизировать с OPCODE_COUNT / op table */
static const char* opcode_names[OPCODE_COUNT] = {
    "ADD","SUB","MUL","DIV","MOD","EXPT","ABS","SQRT","LN","LOG",
    "EXP","SIN","COS","TAN","ASIN","ACOS","ATAN",
    "AND","OR","XOR","NOT",
    "EQ","NE","GT","GE","LT","LE",
    "TIME","DATE","TOD","DT","ADD_TIME","SUB_TIME",
    "YEAR","MONTH","DAY","HOUR","MINUTE","SECOND",
    "LEN","CONCAT","LEFT","RIGHT","MID","INSERT","DELETE","REPLACE",
    "TON","TOF","TP",
    "CTU","CTD","CTUD",
    "LIMIT","SEL","MUX",
    "RISING_EDGE","FALLING_EDGE","EDGE_BOTH","RS_LATCH","SR_LATCH","DEMUX",
    "JMP","JMP_IF","JMP_IF_NOT","EXIT","HALT","NOP"
};
/* --- helper utilities: use explicit C-field helpers --- */
static inline uint8_t instr_opcode(uint32_t instr) {
    return (instr >> 25) & 0x7F;
}
static inline uint8_t instr_ra(uint32_t instr) {
    return (instr >> 17) & 0xFF;
}
static inline uint8_t instr_rb(uint32_t instr) {
    return (instr >> 9) & 0xFF;
}
/* C-field is 9 bits in encoding: [8]=FIMM, [7:0]=imm/reg */
static inline uint16_t instr_cfield(uint32_t instr) {
    return (uint16_t)(instr & 0x1FF);
}
static inline int     instr_has_fimm(uint32_t instr) {
    return ( (instr >> 8) & 1 );
}
static inline int8_t  instr_imm8_signed(uint32_t instr) {
    return (int8_t)(instr & 0xFF); /* sign-extended if cast to int */
}
static inline uint8_t instr_c_as_reg_index(uint32_t instr) {
    return (uint8_t)(instr & 0xFF);
}

/* ----- fprintf_inst: печатаем #imm если FIMM установлен ----- */
void fprintf_inst(FILE *f, uint32_t instr) {
    uint8_t opcode = instr_opcode(instr);
    uint8_t ra = instr_ra(instr);
    uint8_t rb = instr_rb(instr);

    if (instr_has_fimm(instr)) {
        int imm = (int)instr_imm8_signed(instr);
        fprintf(f, "%-10s R%u, R%u, #%d", opcode_name(opcode), ra, rb, imm);
    } else {
        uint8_t rc = instr_c_as_reg_index(instr);
        fprintf(f, "%-10s R%u, R%u, R%u", opcode_name(opcode), ra, rb, rc);
    }
    fprintf(f, "  [0x%08X]", instr);
}

/* Возвращает имя опкода */
const char* opcode_name(uint8_t opcode) {
    if (opcode >= OPCODE_COUNT) return "UNKNOWN";
    const char *s = opcode_names[opcode];
    return s ? s : "UNKNOWN";
}

/* Печать 32-bit в двоичном виде (читабельно) */
void fprintf_binary(FILE *f, uint32_t num) {
    if (!f) return;
    for (int c = 31; c >= 0; c--) {
        if ((c+1) % 8 == 0 && c != 31) fprintf(f, " ");
        fprintf(f, "%d", (int)((num >> c) & 1u));
    }
}

/* dump памяти от from до to (безопасно) */
void fprintf_mem_bytes(FILE *f, uint8_t *mem, uint32_t from, uint32_t to) {
    if (!f || !mem) return;
    if (from >= to) return;
    if (to > VM_MEM_BYTES) to = VM_MEM_BYTES;
    fprintf(f, "Memory dump [0x%08X - 0x%08X]:\n", from, to);
    for (uint32_t i = from; i < to; i += 16) {
        fprintf(f, "  %08X: ", i);
        for (uint32_t j = 0; j < 16 && (i + j) < to; j++) {
            fprintf(f, "%02X ", mem[i + j]);
        }
        fprintf(f, " | ");
        for (uint32_t j = 0; j < 16 && (i + j) < to; j++) {
            uint8_t c = mem[i + j];
            fputc((c >= 32 && c < 127) ? c : '.', f);
        }
        fprintf(f, "\n");
    }
}

/* Печать ненулевых байт памяти (ограничено stop) */
void fprintf_mem_nonzero(FILE *f, uint8_t *mem, uint32_t stop) {
    if (!f || !mem) return;
    if (stop > VM_MEM_BYTES) stop = VM_MEM_BYTES;
    fprintf(f, "Non-zero memory bytes:\n");
    int count = 0;
    for (uint32_t i = 0; i < stop; i++) {
        if (mem[i] != 0) {
            fprintf(f, "  [0x%08X] = 0x%02X (%3d) '%c'\n",
                    i, mem[i], mem[i],
                    isprint(mem[i]) ? mem[i] : '.');
            count++;
        }
    }
    if (count == 0) fprintf(f, "  (none)\n");
}

/* Печать регистра */
void fprintf_reg(FILE *f, uint32_t *reg, int idx) {
    if (!f) return;
    fprintf(f, "R%03d = 0x%08X (%10u)\n", idx, reg[idx], reg[idx]);
}

void fprintf_reg_all(FILE *f, uint32_t *reg, int size) {
    if (!f) return;
    fprintf(f, "All registers:\n");
    for (int i = 0; i < size; i++) {
        if (i % 4 == 0) fprintf(f, "  ");
        fprintf(f, "R%03d=%08X ", i, reg[i]);
        if ((i + 1) % 4 == 0) fprintf(f, "\n");
    }
    if (size % 4 != 0) fprintf(f, "\n");
}

/* Инициализация логирования для конкретного vm */
void init_logging(vm_state_t *vm) {
    if (!vm) return;
    if (!vm->logging_enabled) return;

    const char *fname = vm->verbose_logging ? "vm32_log_verbose.txt" : "vm32_log.txt";
    vm->log_file = fopen(fname, "w");
    if (!vm->log_file) {
        perror("Failed to open log file");
        vm->logging_enabled = false;
        return;
    }

    time_t now = time(NULL);
    fprintf(vm->log_file, "╔═══════════════════════════════════════════════════════════════╗\n");
    fprintf(vm->log_file, "║           VM32 Execution Log - %s", ctime(&now));
    fprintf(vm->log_file, "╠═══════════════════════════════════════════════════════════════╣\n");
    fprintf(vm->log_file, "║ Memory: %llu bytes (%.2f MB)\n",
            (unsigned long long)VM_MEM_BYTES, (double)VM_MEM_BYTES / (1024.0 * 1024.0));
    fprintf(vm->log_file, "║ Registers: %d\n", REG_COUNT);
    fprintf(vm->log_file, "║ Start PC: 0x%08X\n", vm->PC_START);
    fprintf(vm->log_file, "╚═══════════════════════════════════════════════════════════════╝\n\n");
    fflush(vm->log_file);

    /* init debug snapshots */
    vm->dbg_instruction_count = 0;
    vm->dbg_prev_pc = vm->PC;
    memcpy(vm->dbg_prev_reg, vm->reg, sizeof(vm->dbg_prev_reg));
    if (VM_MEM_BYTES >= MEM_LOG_SIZE) memcpy(vm->dbg_prev_mem, vm->mem, MEM_LOG_SIZE);
    else memcpy(vm->dbg_prev_mem, vm->mem, VM_MEM_BYTES);
}

/* Запись одного инстр. (подробно) */
void log_instruction(vm_state_t *vm, uint32_t pc, uint32_t instr) {
    if (!vm || !vm->logging_enabled || !vm->log_file) return;

    fprintf(vm->log_file, "┌─ Instruction #%llu ─────────────────────────────────────────\n",
            (unsigned long long)++vm->dbg_instruction_count);
    fprintf(vm->log_file, "│ PC: 0x%08X\n", pc);
    fprintf(vm->log_file, "│ ");
    fprintf_inst(vm->log_file, instr);
    fprintf(vm->log_file, "\n");
    fprintf(vm->log_file, "│ Binary: ");
    fprintf_binary(vm->log_file, instr);
    fprintf(vm->log_file, "\n");
}

/* Вызывается до исполнения инструкции: сохраняет snapshot и печатает перед-инфо */

/* ----- log_before: print operands carefully and avoid reading vm->reg[rc] when C is immediate ----- */
void log_before(vm_state_t *vm, uint32_t pc, uint32_t instr) {
    if (!vm || !vm->logging_enabled || !vm->log_file) return;

    vm->dbg_prev_pc = pc;
    memcpy(vm->dbg_prev_reg, vm->reg, sizeof(vm->dbg_prev_reg));
    if (VM_MEM_BYTES >= MEM_LOG_SIZE) memcpy(vm->dbg_prev_mem, vm->mem, MEM_LOG_SIZE);
    else memcpy(vm->dbg_prev_mem, vm->mem, VM_MEM_BYTES);

    /* детальный лог инструкции */
    log_instruction(vm, pc, instr);

    uint8_t ra = instr_ra(instr);
    uint8_t rb = instr_rb(instr);
    uint8_t rc_idx = instr_c_as_reg_index(instr);
    int     has_imm = instr_has_fimm(instr);

    fprintf(vm->log_file, "│\n");
    fprintf(vm->log_file, "│ Operands BEFORE:\n");

    /* safe read A */
    if (ra < REG_COUNT) {
        fprintf(vm->log_file, "│   R%u (A) = 0x%08X (%10u)\n", ra, vm->reg[ra], vm->reg[ra]);
    } else {
        fprintf(vm->log_file, "│   R%u (A) = <invalid index>\n", ra);
    }

    /* safe read B */
    if (rb < REG_COUNT) {
        fprintf(vm->log_file, "│   R%u (B) = 0x%08X (%10u)\n", rb, vm->reg[rb], vm->reg[rb]);
    } else {
        fprintf(vm->log_file, "│   R%u (B) = <invalid index>\n", rb);
    }

    /* For C: if immediate -> print signed immediate; else print register value */
    if (has_imm) {
        int imm = (int)instr_imm8_signed(instr);
        /* also show raw low8 for debugging */
        uint8_t raw = (uint8_t)(instr & 0xFF);
        fprintf(vm->log_file, "│   C (immediate) = #%d (raw=0x%02X)\n", imm, raw);
    } else {
        if (rc_idx < REG_COUNT) {
            fprintf(vm->log_file, "│   R%u (C) = 0x%08X (%10u)\n", rc_idx, vm->reg[rc_idx], vm->reg[rc_idx]);
        } else {
            fprintf(vm->log_file, "│   R%u (C) = <invalid index>\n", rc_idx);
        }
    }
    fflush(vm->log_file);
}


/* Вызывается после исполнения инструкции: сравнивает snapshot и пишет изменения */
void log_after(vm_state_t *vm, uint32_t pc) {
    if (!vm || !vm->logging_enabled || !vm->log_file) return;

    fprintf(vm->log_file, "│\n");

    /* Регистры */
    bool reg_changed = false;
    for (int i = 0; i < REG_COUNT; i++) {
        if (vm->dbg_prev_reg[i] != vm->reg[i]) {
            if (!reg_changed) {
                fprintf(vm->log_file, "│ Register CHANGES:\n");
                reg_changed = true;
            }
            fprintf(vm->log_file, "│   R%03d: 0x%08X -> 0x%08X\n",
                    i, vm->dbg_prev_reg[i], vm->reg[i]);
        }
    }
    if (!reg_changed) fprintf(vm->log_file, "│ Register changes: (none)\n");

    /* Память (первые MEM_LOG_SIZE байт) */
    bool mem_changed = false;
    uint32_t mem_limit = (VM_MEM_BYTES < MEM_LOG_SIZE) ? (uint32_t)VM_MEM_BYTES : (uint32_t)MEM_LOG_SIZE;
    for (uint32_t i = 0; i < mem_limit; i++) {
        if (vm->dbg_prev_mem[i] != vm->mem[i]) {
            if (!mem_changed) {
                fprintf(vm->log_file, "│ Memory CHANGES (first %d bytes):\n", MEM_LOG_SIZE);
                mem_changed = true;
            }
            fprintf(vm->log_file, "│   [0x%08X]: 0x%02X -> 0x%02X '%c'\n",
                    i, vm->dbg_prev_mem[i], vm->mem[i],
                    isprint(vm->mem[i]) ? vm->mem[i] : '.');
        }
    }
    if (!mem_changed) fprintf(vm->log_file, "│ Memory changes: (none)\n");

    /* PC */
    fprintf(vm->log_file, "│\n");
    if (vm->dbg_prev_pc != pc) {
        fprintf(vm->log_file, "│ PC changed: 0x%08X -> 0x%08X\n", vm->dbg_prev_pc, pc);
    } else {
        fprintf(vm->log_file, "│ PC: 0x%08X\n", pc);
    }

    fprintf(vm->log_file, "└──────────────────────────────────────────────────────────────\n\n");
    fflush(vm->log_file);
}

/* Закрытие логов */
void close_logging(vm_state_t *vm) {
    if (!vm || !vm->log_file) return;

    fprintf(vm->log_file, "\n╔═══════════════════════════════════════════════════════════════╗\n");
    fprintf(vm->log_file, "║ Execution Summary\n");
    fprintf(vm->log_file, "╠═══════════════════════════════════════════════════════════════╣\n");
    fprintf(vm->log_file, "║ Total instructions executed: %llu\n", (unsigned long long)vm->dbg_instruction_count);
    fprintf(vm->log_file, "║ Final PC: 0x%08X\n", vm->PC);
    fprintf(vm->log_file, "╚═══════════════════════════════════════════════════════════════╝\n");

    fclose(vm->log_file);
    vm->log_file = NULL;
}
