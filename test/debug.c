#define _POSIX_C_SOURCE 200809L  // можно и без этого, если не требуется
#include "vm32.h"
#include "debug.h"
#include "funcs.h"

#include <string.h>
#include <time.h>

// Глобальные переменные
bool logging_enabled = true;
bool verbose_logging = true;  // по умолчанию выключе
FILE *log_file = NULL;

// Состояние до выполнения инструкции
static uint32_t prev_reg[REG_COUNT];
static uint8_t prev_mem[MEM_LOG_SIZE];
static uint32_t prev_pc = 0;
static uint64_t instruction_count = 0;

// Таблица имен опкодов
static const char* opcode_names[OPCODE_COUNT] = {
    "ADD", "SUB", "MUL", "DIV", "MOD", "EXPT", "ABS", "SQRT", "LN", "LOG",
    "EXP", "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN",
    "AND", "OR", "XOR", "NOT",
    "EQ", "NE", "GT", "GE", "LT", "LE",
    "TIME", "DATE", "TOD", "DT", "ADD_TIME", "SUB_TIME",
    "YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "SECOND",
    "LEN", "CONCAT", "LEFT", "RIGHT", "MID", "INSERT", "DELETE", "REPLACE",
    "TON", "TOF", "TP",
    "CTU", "CTD", "CTUD",
    "LIMIT", "SEL", "MUX",
    "JMP", "JMP_IF", "JMP_IF_NOT",
    "HALT", "EXIT"
};


const char* opcode_name(uint8_t opcode) {
    if (opcode >= OPCODE_COUNT) return "UNKNOWN";
    return opcode_names[opcode];
}

void fprintf_binary(FILE *f, uint32_t num) {
    for (int c = 31; c >= 0; c--) {
        if ((c+1) % 8 == 0 && c != 31) fprintf(f, " ");
        fprintf(f, "%d", (num >> c) & 1);
    }
}

void fprintf_inst(FILE *f, uint32_t instr) {
    uint8_t opcode = OPC(instr);
    uint8_t ra = RA(instr);
    uint8_t rb = RB(instr);
    uint32_t rc = RC(instr);
    
    fprintf(f, "%-10s R%d, R%d, R%d", opcode_name(opcode), ra, rb, rc);
    fprintf(f, "  [0x%08X]", instr);
}

// Извлекает поля opcode, A, B, C из 32-битного слова
static inline void disassemble(uint32_t instr,
                               uint8_t *opcode,
                               uint8_t *A,
                               uint8_t *B,
                               uint16_t *C)
{
    *opcode = (instr >> 25) & 0x7F;
    *A      = (instr >> 17) & 0xFF;
    *B      = (instr >>  9) & 0xFF;
    *C      = instr & 0x1FF;
}


void fprintf_mem_bytes(FILE *f, uint8_t *mem, uint32_t from, uint32_t to) {
    fprintf(f, "Memory dump [0x%04X - 0x%04X]:\n", from, to);
    for (uint32_t i = from; i < to; i += 16) {
        fprintf(f, "  %04X: ", i);
        // Hex dump
        for (uint32_t j = 0; j < 16 && (i + j) < to; j++) {
            fprintf(f, "%02X ", mem[i + j]);
        }
        // ASCII dump
        fprintf(f, " | ");
        for (uint32_t j = 0; j < 16 && (i + j) < to; j++) {
            uint8_t c = mem[i + j];
            fprintf(f, "%c", (c >= 32 && c < 127) ? c : '.');
        }
        fprintf(f, "\n");
    }
}

void fprintf_mem_nonzero(FILE *f, uint8_t *mem, uint32_t stop) {
    fprintf(f, "Non-zero memory bytes:\n");
    int count = 0;
    for (uint32_t i = 0; i < stop; i++) {
        if (mem[i] != 0) {
            fprintf(f, "  [0x%04X] = 0x%02X (%3d) '%c'\n", 
                    i, mem[i], mem[i], 
                    (mem[i] >= 32 && mem[i] < 127) ? mem[i] : '.');
            count++;
        }
    }
    if (count == 0) {
        fprintf(f, "  (none)\n");
    }
}

void fprintf_reg(FILE *f, uint32_t *reg, int idx) {
    fprintf(f, "R%03d = 0x%08X (%10u)\n", idx, reg[idx], reg[idx]);
}

void fprintf_reg_all(FILE *f, uint32_t *reg, int size) {
    fprintf(f, "All registers:\n");
    for (int i = 0; i < size; i++) {
        if (i % 4 == 0) fprintf(f, "  ");
        fprintf(f, "R%03d=%08X ", i, reg[i]);
        if ((i + 1) % 4 == 0) fprintf(f, "\n");
    }
    if (size % 4 != 0) fprintf(f, "\n");
}

void init_logging() {
    if (!logging_enabled) return;
    
    log_file = fopen("vm32_log.txt", "w");
    if (!log_file) {
        perror("Failed to open log file");
        logging_enabled = false;
        return;
    }
    
    time_t now = time(NULL);
    fprintf(log_file, "╔═══════════════════════════════════════════════════════════════╗\n");
    fprintf(log_file, "║           VM32 Execution Log - %s", ctime(&now));
    fprintf(log_file, "╠═══════════════════════════════════════════════════════════════╣\n");
    fprintf(log_file, "║ Memory: %llu bytes (%.2f MB)\n", VM_MEM_BYTES, VM_MEM_BYTES / (1024.0 * 1024.0));
    fprintf(log_file, "║ Registers: %d\n", NUM_REGS);
    fprintf(log_file, "║ Start PC: 0x%04X\n", PC_START);
    fprintf(log_file, "╚═══════════════════════════════════════════════════════════════╝\n\n");
    fflush(log_file);
    
    instruction_count = 0;
}

void log_instruction(uint32_t pc, uint32_t instr) {
    if (!logging_enabled || !log_file) return;
    
    fprintf(log_file, "┌─ Instruction #%lu ─────────────────────────────────────────\n", 
            ++instruction_count);
    fprintf(log_file, "│ PC: 0x%04X\n", pc);
    fprintf(log_file, "│ ");
    fprintf_inst(log_file, instr);
    fprintf(log_file, "\n");
    fprintf(log_file, "│ Binary: ");
    fprintf_binary(log_file, instr);
    fprintf(log_file, "\n");
}
void log_before(uint32_t pc, uint32_t instr) {
    if (!logging_enabled || !log_file) return;
    
    prev_pc = pc;
    memcpy(prev_reg, reg, sizeof(prev_reg));
    memcpy(prev_mem, mem, MEM_LOG_SIZE);

    
    // Детальный лог только если включен
    if (!verbose_logging) {
        // Краткий лог: только номер инструкции и PC
        if (instruction_count % 100 == 0) {
            fprintf(log_file, "[#%lu] PC=0x%04X %s\n", 
                    instruction_count, pc, opcode_name(OPC(instr)));
        }
        return;
    }
    
    // Детальный лог (оригинальный код)
    log_instruction(pc, instr);
    
    uint8_t ra = RA(instr);
    uint8_t rb = RB(instr);
    uint32_t rc = RC(instr);
    
    fprintf(log_file, "│\n");
    fprintf(log_file, "│ Operands BEFORE:\n");
    fprintf(log_file, "│   R%d (A) = 0x%08X (%u)\n", ra, reg[ra], reg[ra]);
    fprintf(log_file, "│   R%d (B) = 0x%08X (%u)\n", rb, reg[rb], reg[rb]);
    fprintf(log_file, "│   R%d (C) = 0x%08X (%u)\n", rc, reg[rc], reg[rc]);
    
    fflush(log_file);
}


void log_after(uint32_t pc) {
    if (!logging_enabled || !log_file || !verbose_logging) return;
    
    fprintf(log_file, "│\n");
    
    // Логируем изменения регистров
    bool reg_changed = false;
    for (int i = 0; i < NUM_REGS; i++) {
        if (prev_reg[i] != reg[i]) {
            if (!reg_changed) {
                fprintf(log_file, "│ Register CHANGES:\n");
                reg_changed = true;
            }
            fprintf(log_file, "│   R%03d: 0x%08X (%10u) -> 0x%08X (%10u)\n", 
                    i, prev_reg[i], prev_reg[i], reg[i], reg[i]);
        }
    }
    if (!reg_changed) {
        fprintf(log_file, "│ Register changes: (none)\n");
    }
    
    // Логируем изменения памяти
    fprintf(log_file, "│\n");
    bool mem_changed = false;
    for (uint32_t i = 0; i < MEM_LOG_SIZE && i < MEM_BYTES; i++) {
        if (prev_mem[i] != mem[i]) {
            if (!mem_changed) {
                fprintf(log_file, "│ Memory CHANGES (first %d bytes):\n", MEM_LOG_SIZE);
                mem_changed = true;
            }
            fprintf(log_file, "│   [0x%04X]: 0x%02X (%3d) -> 0x%02X (%3d) '%c'\n", 
                    i, prev_mem[i], prev_mem[i], mem[i], mem[i],
                    (mem[i] >= 32 && mem[i] < 127) ? mem[i] : '.');
        }
    }
    if (!mem_changed) {
        fprintf(log_file, "│ Memory changes: (none)\n");
    }
    
    // Логируем новый PC
    fprintf(log_file, "│\n");
    if (prev_pc != pc) {
        fprintf(log_file, "│ PC changed: 0x%04X -> 0x%04X\n", prev_pc, pc);
    } else {
        fprintf(log_file, "│ PC: 0x%04X\n", pc);
    }
    
    fprintf(log_file, "└──────────────────────────────────────────────────────────────\n\n");
    fflush(log_file);
}

void close_logging() {
    if (!log_file) return;
    
    fprintf(log_file, "\n╔═══════════════════════════════════════════════════════════════╗\n");
    fprintf(log_file, "║ Execution Summary\n");
    fprintf(log_file, "╠═══════════════════════════════════════════════════════════════╣\n");
    fprintf(log_file, "║ Total instructions executed: %lu\n", instruction_count);
    fprintf(log_file, "║ Final PC: 0x%04X\n", PC);
    fprintf(log_file, "╚═══════════════════════════════════════════════════════════════╝\n");
    
    fclose(log_file);
    log_file = NULL;
}