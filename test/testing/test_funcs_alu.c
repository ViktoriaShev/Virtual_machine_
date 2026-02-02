#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/vm32.h"
#include "../include/funcs.h"  // содержит OPC/RA/RB/... и sext()

/* Мы будем линковать тест с vm32.c (reg, mem, ...) и funcs.c */

/* ------ Тест-харнесc ------ */
static int tests = 0;
static int fails = 0;
#define REPORT(name, cond) do { \
    tests++; \
    if (cond) printf("PASS: %s\n", name); \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fails++; } \
} while(0)

/* helper: собрать инструкцию */
static inline uint32_t build_instr(uint8_t opcode, uint8_t ra, uint8_t rb, int fimm, uint8_t c) {
    return ((uint32_t)opcode << 25) | ((uint32_t)ra << 17) | ((uint32_t)rb << 9) | ((uint32_t)(fimm & 1) << 8) | (uint32_t)c;
}

/* Номера опкодов (сопоставлены по order в vm32.c op_ex array) */
enum {
    OP_ADD = 0, OP_SUB = 1, OP_MUL = 2, OP_DIV = 3, OP_MOD = 4, OP_EXPT = 5,
    OP_ABS = 6, OP_SQRT = 7,
    OP_AND = 17, OP_OR = 18, OP_XOR = 19, OP_NOT = 20,
    OP_EQ = 21, OP_NE = 22, OP_GT = 23, OP_GE = 24, OP_LT = 25, OP_LE = 26
};

static void reset_vm_state(void) {
    if (mem) free(mem);
    mem = (uint8_t *)calloc(1, MEM_BYTES);
    for (int i = 0; i < REG_COUNT; ++i) reg[i] = 0;
}

/* Тесты */
static void test_add_sub_mul(void) {
    reset_vm_state();
    reg[1] = 5; reg[2] = 3;
    uint32_t instr = build_instr(OP_ADD, 10, 1, 0, 2); // add R10 = R1 + R2
    op_add(instr);
    REPORT("add: 5+3 == 8", reg[10] == 8u);

    instr = build_instr(OP_SUB, 11, 1, 0, 2); // sub R11 = R1 - R2
    op_sub(instr);
    REPORT("sub: 5-3 == 2", reg[11] == 2u);

    instr = build_instr(OP_MUL, 12, 1, 0, 2);
    op_mul(instr);
    REPORT("mul: 5*3 == 15", reg[12] == 15u);
}

static void test_div_mod_and_divbyzero(void) {
    reset_vm_state();
    reg[1] = 10; reg[2] = 2;
    uint32_t instr = build_instr(OP_DIV, 20, 1, 0, 2); // div R20 = R1 / R2
    op_div(instr);
    REPORT("div: 10/2 == 5", reg[20] == 5u);

    instr = build_instr(OP_MOD, 21, 1, 0, 2);
    op_mod(instr);
    REPORT("mod: 10%2 == 0", reg[21] == 0u);

    /* div by zero: C == 0 => defined to set result 0 */
    reg[2] = 0;
    instr = build_instr(OP_DIV, 22, 1, 0, 2);
    op_div(instr);
    REPORT("div by zero => 0", reg[22] == 0u);

    instr = build_instr(OP_MOD, 23, 1, 0, 2);
    op_mod(instr);
    REPORT("mod by zero => 0", reg[23] == 0u);
}

static void test_expt_abs_sqrt(void) {
    reset_vm_state();
    reg[1] = 2;
    /* use immediate exponent 3: Cv_or_imm uses SEXTIMM8 when FIMM==1 */
    uint32_t instr = build_instr(OP_EXPT, 30, 1, 1, 3); // 2^3 = 8
    op_expt(instr);
    REPORT("expt (2^3) == 8", reg[30] == 8u);

    reg[1] = (uint32_t)(-7); // as uint32_t two's complement
    instr = build_instr(OP_ABS, 31, 1, 0, 0);
    op_abs(instr);
    REPORT("abs(-7) == 7", reg[31] == 7u);

    reg[1] = 16;
    instr = build_instr(OP_SQRT, 32, 1, 0, 0);
    op_sqrt(instr);
    REPORT("sqrt(16) == 4", reg[32] == 4u);
}

static void test_bitwise_ops(void) {
    reset_vm_state();
    reg[1] = 0xF0F0; reg[2] = 0x0FF0;
    uint32_t instr = build_instr(OP_AND, 40, 1, 0, 2);
    op_and(instr);
    REPORT("and: check", reg[40] == (reg[1] & reg[2]));

    instr = build_instr(OP_OR, 41, 1, 0, 2);
    op_or(instr);
    REPORT("or: check", reg[41] == (reg[1] | reg[2]));

    instr = build_instr(OP_XOR, 42, 1, 0, 2);
    op_xor(instr);
    REPORT("xor: check", reg[42] == (reg[1] ^ reg[2]));

    instr = build_instr(OP_NOT, 43, 1, 0, 0);
    op_not(instr);
    REPORT("not: check", reg[43] == (~reg[1]));
}

static void test_comparisons(void) {
    reset_vm_state();
    reg[1] = 5; reg[2] = 3;
    uint32_t instr;

    instr = build_instr(OP_EQ, 50, 1, 0, 2); op_eq(instr);
    REPORT("eq: 5==3 false", reg[50] == 0u);

    instr = build_instr(OP_NE, 51, 1, 0, 2); op_ne(instr);
    REPORT("ne: 5!=3 true", reg[51] == 1u);

    instr = build_instr(OP_GT, 52, 1, 0, 2); op_gt(instr);
    REPORT("gt: 5>3 true", reg[52] == 1u);

    instr = build_instr(OP_GE, 53, 1, 0, 2); op_ge(instr);
    REPORT("ge: 5>=3 true", reg[53] == 1u);

    instr = build_instr(OP_LT, 54, 2, 0, 1); op_lt(instr); // 3 < 5 true
    REPORT("lt: 3<5 true", reg[54] == 1u);

    instr = build_instr(OP_LE, 55, 2, 0, 1); op_le(instr); // 3 <= 5 true
    REPORT("le: 3<=5 true", reg[55] == 1u);
}

/* test immediate sign-extension behavior for Cv_or_imm */
static void test_immediate_sext(void) {
    reset_vm_state();
    reg[1] = 10;
    /* use immediate -1 (0xFF) with FIMM=1 => SEXTIMM8 => -1 */
    uint32_t instr = build_instr(OP_ADD, 60, 1, 1, 0xFF);
    op_add(instr); // reg[60] = reg[1] + (-1) => 9
    REPORT("add with imm -1 -> 9", reg[60] == 9u);
}

/* main */
int main(void) {
    printf("=== funcs ALU unit tests ===\n");

    test_add_sub_mul();
    test_div_mod_and_divbyzero();
    test_expt_abs_sqrt();
    test_bitwise_ops();
    test_comparisons();
    test_immediate_sext();

    printf("=================================\n");
    if (fails == 0) {
        printf("ALL TESTS PASSED (%d)\n", tests);
        return 0;
    } else {
        printf("FAILED: %d / %d\n", fails, tests);
        return 1;
    }
}
