#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/vm32.h"
#include "../include/funcs.h"
#include "../include/cleanup.h"

/* tiny harness */
static int tests = 0;
static int fails = 0;
#define REPORT(name, cond) do { tests++; if (cond) printf("PASS: %s\n", name); else { printf("FAIL: %s (line %d)\n", name, __LINE__); fails++; } } while(0)

/* Build instruction helper */
static inline uint32_t build_instr(uint8_t opcode, uint8_t ra, uint8_t rb, int fimm, uint8_t c) {
    return ((uint32_t)opcode << 25) | ((uint32_t)ra << 17) | ((uint32_t)rb << 9) | ((uint32_t)(fimm & 1) << 8) | (uint32_t)c;
}

/* Opcodes (from vm32.c op_ex array ordering) */
enum {
    /* ... skip to jmp family ... */
    OP_JMP = 56,
    OP_JMP_IF = 57,
    OP_JMP_IF_NOT = 58,
    OP_EXIT = 59,
    OP_HALT = 60
};

static void reset_vm_state(void) {
    if (mem) free(mem);
    mem = (uint8_t*)calloc(1, MEM_BYTES);
    for (int i = 0; i < REG_COUNT; ++i) reg[i] = 0;
    running = true;
}

/* Tests */
static void test_jmp_immediate_and_reg(void) {
    reset_vm_state();

    /* immediate (SEXTIMM8) -> small signed address */
    uint32_t instr = build_instr(OP_JMP, 0, 0, 1, 0x10); /* immediate 16 */
    PC = 0x2000;
    op_jmp(instr);
    REPORT("jmp immediate sets PC to immediate (16)", PC == (uint32_t)0x10);

    /* via register B (Bv) */
    reset_vm_state();
    reg[2] = 0x3000;
    instr = build_instr(OP_JMP, 0, 2, 0, 0); /* use Bv */
    PC = 0x0;
    op_jmp(instr);
    REPORT("jmp via register sets PC to reg[RB]", PC == 0x3000);
}

static void test_jmp_if_and_jmp_if_not(void) {
    reset_vm_state();

    /* jmp_if: condition in C (Cv) */
    reg[3] = 0x1234; // target value if using Bv or whatever; we will test immediate and reg forms
    /* test when Cv (reg[RC]) == 0 => no jump */
    reg[5] = 0; // RC will point to 5
    uint32_t instr = build_instr(OP_JMP_IF, 0, 1, 0, 5); // FIMM=0 => Cv == reg[5] == 0
    PC = 0xAAAA;
    op_jmp_if(instr);
    REPORT("jmp_if does not jump when condition false", PC == 0xAAAA);

    /* when Cv != 0 -> jump to Bv */
    reg[5] = 1;
    reg[1] = 0x4242; // B register used as target (Bv)
    instr = build_instr(OP_JMP_IF, 0, 1, 0, 5);
    PC = 0x0;
    op_jmp_if(instr);
    REPORT("jmp_if jumps when condition true", PC == 0x4242);

    /* jmp_if_not: jump when condition is false */
    reset_vm_state();
    reg[7] = 0; // RC==7 is zero
    reg[2] = 0x5555; // B target
    instr = build_instr(OP_JMP_IF_NOT, 0, 2, 0, 7);
    PC = 0x1111;
    op_jmp_if_not(instr);
    REPORT("jmp_if_not jumps when cond false", PC == 0x5555);

    reg[7] = 1;
    PC = 0x1111;
    instr = build_instr(OP_JMP_IF_NOT, 0, 2, 0, 7);
    op_jmp_if_not(instr);
    REPORT("jmp_if_not does not jump when cond true", PC == 0x1111);
}

static void test_exit_and_halt_behavior(void) {
    reset_vm_state();

    /* op_exit: if Av == 0 -> take immediate IMM8 as code */
    reg[1] = 0; /* Av(i) == 0 */
    uint32_t instr = build_instr(OP_EXIT, 1, 0, 1, 7); /* RA=1, IMM=7 */
    vm_exit_code = 0xDEADBEEF;
    running = true;
    op_exit(instr);
    REPORT("exit with Av==0 uses immediate (7)", vm_exit_code == 7 && running == false);

    /* op_exit: if Av != 0 -> current code uses A(i) (register index) per current funcs.c */
    reset_vm_state();
    reset_vm_state();
    reg[9] = 1; /* Av != 0 for RA==9 */
    instr = build_instr(OP_EXIT, 9, 0, 1, 3); /* RA field 9, IMM=3 */
    vm_exit_code = 0;
    running = true;
    op_exit(instr);
    REPORT("exit with Av!=0 yields code == A(i) (register index) (behavior check)", vm_exit_code == 9 && running == false);


    /* op_halt should set running=false (and request stop) */
    reset_vm_state();
    running = true;
    instr = build_instr(OP_HALT, 0, 0, 0, 0);
    op_halt(instr);
    REPORT("halt sets running=false", running == false);
}

int main(void) {
    printf("=== funcs JMP / control unit tests ===\n");

    test_jmp_immediate_and_reg();
    test_jmp_if_and_jmp_if_not();
    test_exit_and_halt_behavior();

    printf("=====================================\n");
    if (fails == 0) {
        printf("ALL JMP TESTS PASSED (%d)\n", tests);
        return 0;
    } else {
        printf("FAILED: %d / %d\n", fails, tests);
        return 1;
    }
}
