#ifndef EXECUTION_TEST_HELPERS_H
#define EXECUTION_TEST_HELPERS_H

#include "main.h"
#include "funcs.h"

#include <stddef.h>
#include <stdint.h>

enum {
    OP_ADD = 0,
    OP_SUB = 1,
    OP_MUL = 2,
    OP_DIV = 3,
    OP_DEMUX = 61,
    OP_JMP = 62,
    OP_JMP_IF = 63,
    OP_JMP_IF_NOT = 64,
    OP_EXIT = 65,
    OP_HALT = 66,
    OP_NOP = 67
};

#define ENC_RRR(op, a, b, c) \
    ((((uint32_t)((op) & 0x7F)) << 25) | (((uint32_t)((a) & 0xFF)) << 17) | (((uint32_t)((b) & 0xFF)) << 9) | ((uint32_t)((c) & 0xFF)))

#define ENC_RRI(op, a, b, imm8) \
    ((((uint32_t)((op) & 0x7F)) << 25) | (((uint32_t)((a) & 0xFF)) << 17) | (((uint32_t)((b) & 0xFF)) << 9) | (1u << 8) | ((uint32_t)((uint8_t)(imm8))))

static inline void load_program_words(vm_state_t *vm, uint32_t start_addr, const uint32_t *words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        vm_mw32(vm, start_addr + (uint32_t)(i * 4), words[i]);
    }
    vm->PC_START = start_addr;
    vm->PC = start_addr;
}

static inline int vm_step_for_test(vm_state_t *vm, uint32_t start_addr, uint32_t end_addr) {
    if (!vm || vm->PC < start_addr || vm->PC >= end_addr) {
        return 1;
    }

    uint32_t instr = vm_mr32(vm, vm->PC);
    if (instr == 0) {
        return 1;
    }

    uint8_t opcode = OPC(instr);
    if (opcode >= OPCODE_COUNT || vm->op_ex[opcode] == NULL) {
        vm->running = false;
        return -1;
    }

    uint32_t old_pc = vm->PC;
    vm->op_ex[opcode](vm, instr);

    if (vm->PC == old_pc) {
        vm->PC += 4;
    }

    return 0;
}

static inline int vm_run_for_test(vm_state_t *vm, uint32_t start_addr, size_t instr_words, uint32_t max_steps, uint32_t *steps_done) {
    uint32_t steps = 0;
    uint32_t end_addr = start_addr + (uint32_t)(instr_words * 4);

    vm->running = true;
    while (vm->running && steps < max_steps) {
        int rc = vm_step_for_test(vm, start_addr, end_addr);
        if (rc != 0) {
            if (steps_done) *steps_done = steps;
            return (rc == 1) ? 0 : rc;
        }
        steps++;
    }

    if (steps_done) *steps_done = steps;
    return vm->running ? 2 : 0;
}

#endif
