// debug_vm.h
#pragma once
#include <stdio.h>
#include <stdint.h>

void fprintf_binary(FILE *f, uint32_t num);
void fprintf_inst(FILE *f, uint32_t instr);
void fprintf_mem(FILE *f, uint32_t *mem, uint32_t from, uint32_t to);
void fprintf_mem_nonzero(FILE *f, uint32_t *mem, uint32_t stop);
void fprintf_reg(FILE *f, uint32_t *reg, int idx);
void fprintf_reg_all(FILE *f, uint32_t *reg, int size);
