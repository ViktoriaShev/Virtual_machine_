// vm.c (фрагмент)
#include "mvp.h"
#include <string.h>
#include <stdio.h>

VM* vm_create(size_t mem_size) {
    VM* vm = calloc(1, sizeof(VM));
    vm->mem = calloc(1, mem_size);
    vm->ip = 0;
    vm->sp = mem_size - 4; // простой стек
    vm->running = 1;
    return vm;
}
void vm_destroy(VM* vm) {
    if (!vm) return;
    free(vm->mem);
    free(vm);
}

// opcode examples
enum OPCODES {
    OP_NOP = 0x00,
    OP_ADD = 0x01, // R-type: rd, rs1, rs2
    OP_SUB = 0x02,
    OP_MOV = 0x10, // I-type: rd, rs1, imm
    OP_HALT = 0xFF
};

static inline int get_rd(uint32_t instr){ return (instr >> 19) & 0x1F; }
static inline int get_rs1(uint32_t instr){ return (instr >> 14) & 0x1F; }
static inline int get_rs2(uint32_t instr){ return (instr >> 9) & 0x1F; }
static inline int get_opcode(uint32_t instr){ return (instr >> 24) & 0xFF; }
static inline int get_imm14(uint32_t instr){
    int32_t imm = instr & 0x3FFF;
    // sign extend 14-bit
    if (imm & (1<<13)) imm |= ~0x3FFF;
    return imm;
}

void handler_add(VM* vm, uint32_t instr){
    int rd = get_rd(instr), rs1 = get_rs1(instr), rs2 = get_rs2(instr);
    vm->regs[rd] = vm->regs[rs1] + vm->regs[rs2];
    vm->ip += 4;
}
void handler_mov_imm(VM* vm, uint32_t instr){
    int rd = get_rd(instr), rs1 = get_rs1(instr);
    int imm = get_imm14(instr);
    // MOV rd, rs1, imm  => rd = rs1 + imm
    vm->regs[rd] = vm->regs[rs1] + imm;
    vm->ip += 4;
}
void handler_halt(VM* vm, uint32_t instr){
    vm->running = 0;
}

void vm_run(VM* vm) {
    // initialize table
    for (int i=0;i<256;i++) vm->opcode_handlers[i] = NULL;
    vm->opcode_handlers[OP_ADD] = handler_add;
    vm->opcode_handlers[OP_MOV] = handler_mov_imm;
    vm->opcode_handlers[OP_HALT] = handler_halt;

    while(vm->running) {
        uint32_t instr = vm_fetch32(vm);
        int op = get_opcode(instr);
        if (vm->opcode_handlers[op]) {
            vm->opcode_handlers[op](vm, instr);
        } else {
            printf("Unhandled opcode 0x%02X at ip=0x%08X\n", op, vm->ip);
            vm->running = 0;
        }
    }
}
