// debug_vm.c
#include "debug.h"
#include <stdio.h>

void fprintf_binary(FILE *f, uint32_t num) {
    for (int c = 31; c >= 0; c--) {
        if ((c+1)%4 == 0) fprintf(f, " ");
        fprintf(f, "%d", (num >> c) & 1);
    }
}

void fprintf_inst(FILE *f, uint32_t instr) {
    fprintf(f, "instr=%u, binary=", instr);
    fprintf_binary(f, instr);
    fprintf(f, "\n");
}

void fprintf_mem(FILE *f, uint32_t *mem, uint32_t from, uint32_t to) {
    for(uint32_t i = from; i < to; i++) {
        fprintf(f, "mem[%u|0x%.08x]=", i, i);
        fprintf_binary(f, mem[i]);
        fprintf(f, "\n");
    }
}

void fprintf_mem_nonzero(FILE *f, uint32_t *mem, uint32_t stop) {
    for(uint32_t i = 0; i < stop; i++) {
        if(mem[i] != 0) {
            fprintf(f, "mem[%u|0x%.08x]=", i, i);
            fprintf_binary(f, mem[i]);
            fprintf(f, "\n");
        }
    }
}

void fprintf_reg(FILE *f, uint32_t *reg, int idx) {
    fprintf(f, "reg[%d]=0x%.08x\n", idx, reg[idx]);
}

void fprintf_reg_all(FILE *f, uint32_t *reg, int size) {
    for(int i = 0; i < size; i++) {
        fprintf_reg(f, reg, i);
    }
}
