#define _POSIX_C_SOURCE 200809L

#include "test_common.h"
#include "vm_lifecycle.h"
#include "hashing.h"
#include "instruction/execution_test_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t rand_u32(void) {
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

static uint32_t random_safe_opcode(void) {
    static const uint32_t ops[] = {
        OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
        OP_AND, OP_OR, OP_XOR, OP_NOT,
        OP_EQ, OP_NE, OP_GT, OP_GE, OP_LT, OP_LE,
        OP_NOP
    };

    return ops[rand() % (sizeof(ops) / sizeof(ops[0]))];
}

static uint32_t random_instr_word(void) {
    uint32_t op = random_safe_opcode();
    uint32_t a = (uint32_t)(rand() % REG_COUNT);
    uint32_t b = (uint32_t)(rand() % REG_COUNT);

    if (op == OP_NOT || op == OP_NOP) {
        return ENC_RRR(op, a, b, 0);
    }

    if ((rand() & 1) != 0) {
        uint32_t c = (uint32_t)(rand() % REG_COUNT);
        return ENC_RRR(op, a, b, c);
    }

    return ENC_RRI(op, a, b, (uint8_t)rand_u32());
}

static void fill_random_regs(vm_state_t *vm, uint32_t seed) {
    srand(seed);
    for (size_t i = 0; i < REG_COUNT; ++i) {
        vm->reg[i] = rand_u32();
    }
}

static int run_case(uint32_t seed) {
    srand(seed);

    size_t body_len = 8u + (size_t)(rand() % 24);
    size_t total_len = body_len + 1u;

    uint32_t *program = (uint32_t *)calloc(total_len, sizeof(uint32_t));
    ASSERT_NOT_NULL(program);

    for (size_t i = 0; i < body_len; ++i) {
        program[i] = random_instr_word();
    }
    program[body_len] = ENC_RRR(OP_HALT, 0, 0, 0);

    vm_state_t *vm1 = vm_create();
    vm_state_t *vm2 = vm_create();
    ASSERT_NOT_NULL(vm1);
    ASSERT_NOT_NULL(vm2);

    fill_random_regs(vm1, seed ^ 0xA5A5A5A5u);
    memcpy(vm2->reg, vm1->reg, sizeof(vm1->reg));

    load_program_words(vm1, 0x3000, program, total_len);
    load_program_words(vm2, 0x3000, program, total_len);

    uint32_t steps1 = 0;
    uint32_t steps2 = 0;

    int rc1 = vm_run_for_test(vm1, 0x3000, total_len, (uint32_t)(total_len + 4u), &steps1);
    int rc2 = vm_run_for_test(vm2, 0x3000, total_len, (uint32_t)(total_len + 4u), &steps2);

    ASSERT_EQ_U32(0, (uint32_t)rc1);
    ASSERT_EQ_U32(0, (uint32_t)rc2);
    ASSERT_EQ_U32((uint32_t)total_len, steps1);
    ASSERT_EQ_U32(steps1, steps2);

    uint32_t h1 = calculate_registers_hash_ex(vm1->reg, REG_COUNT, HASH_CRC32);
    uint32_t h2 = calculate_registers_hash_ex(vm2->reg, REG_COUNT, HASH_CRC32);
    ASSERT_EQ_U32(h1, h2);

    vm_destroy(vm1);
    vm_destroy(vm2);
    free(program);
    return 0;
}

static int test_random_program_stress(void) {
    for (uint32_t i = 0; i < 200; ++i) {
        ASSERT_EQ_U32(0, run_case(0xC0FFEEu + i * 17u));
    }
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_random_program_stress());
    printf("[PASS] vm_random_program_stress_test\n");
    return 0;
}