#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "../include/vm32.h"
#include "funcs.h"   /* для доступа к ctu_counters/ctd_counters/ctud_counters extern */

/* Test harness */
static int tests = 0;
static int fails = 0;
#define REPORT(name, cond) \
    do { tests++; if (cond) printf("PASS: %s\n", name); else { printf("FAIL: %s (line %d)\n", name, __LINE__); fails++; } } while(0)

/* helper to build instruction word */
static inline uint32_t make_instr(uint8_t opcode, uint8_t ra, uint8_t rb, bool imm, uint8_t c) {
    return ((uint32_t)opcode << 25) |
           ((uint32_t)ra << 17) |
           ((uint32_t)rb << 9) |
           ((uint32_t)(imm ? 1 : 0) << 8) |
           ((uint32_t)c & 0xFFu);
}

/* Forward: test-only reset helper (must be provided by funcs.c under UNIT_TEST) */
extern void vm_counters_reset(void);

/* Opcodes (must match order in vm32.c op_ex table) */
enum {
    OPC_CTU  = 50,
    OPC_CTD  = 51,
    OPC_CTUD = 52,
};

/* Tests */

static void test_ctu_basic(void) {
    vm_counters_reset();

    const uint8_t id = 0;
    const uint8_t reg_id = 0; /* RA index */
    const uint8_t reg_in = 1; /* RB index */
    const uint8_t reg_tmp = 2;

    /* preset = 2 (immediate) */
    uint32_t instr = make_instr(OPC_CTU, reg_id, reg_in, true, 2);

    /* ensure counters start at 0 */
    ctu_counters[id].value = 0;

    /* ensure RA contains id before invocation (op will overwrite RA; restore between calls) */
    reg[reg_id] = id;
    reg[reg_in] = 0;
    op_ctu(instr); /* no edge */
    REPORT("ctu: no rising -> stays 0", ctu_counters[id].value == 0 && reg[reg_id] == 0);

    /* first rising */
    reg[reg_id] = id;
    reg[reg_in] = 1;
    op_ctu(instr);
    REPORT("ctu: first rising increments to 1", ctu_counters[id].value == 1);

    /* falling edge (should do nothing) */
    reg[reg_id] = id;
    reg[reg_in] = 0;
    op_ctu(instr);
    REPORT("ctu: falling does not increment", ctu_counters[id].value == 1);

    /* second rising -> reach preset */
    reg[reg_id] = id;
    reg[reg_in] = 1;
    op_ctu(instr);
    REPORT("ctu: second rising increments to 2", ctu_counters[id].value == 2);

    /* After hitting preset(2) Q should be true (op writes Q into reg[RA]) */
    reg[reg_id] = id;
    reg[reg_in] = 1;
    op_ctu(instr);
    /* op_ctu writes reg[RA] = (value >= preset) ? 1 : 0; restore reg[reg_id] to read Q */
    REPORT("ctu: Q true after reaching preset", reg[reg_id] == 1);
}

static void test_ctu_limits(void) {
    vm_counters_reset();

    const uint8_t id = 1;
    const uint8_t ra = 3, rb = 4;
    uint32_t instr = make_instr(OPC_CTU, ra, rb, true, 0 /*preset=0*/);

    /* set counter to UINT32_MAX and try to increment */
    ctu_counters[id].value = UINT32_MAX;
    /* ensure prev_input for id is 0 (vm_counters_reset did it) */
    reg[ra] = id;
    reg[rb] = 0;
    op_ctu(instr); /* no rising */
    reg[ra] = id;
    reg[rb] = 1;
    op_ctu(instr); /* rising — should NOT overflow */
    REPORT("ctu: does not overflow at UINT32_MAX", ctu_counters[id].value == UINT32_MAX);
}

static void test_ctd_basic_and_limits(void) {
    vm_counters_reset();

    const uint8_t id = 2;
    const uint8_t ra = 5, rb = 6;

    /* preset = 1 (immediate) */
    uint32_t instr = make_instr(OPC_CTD, ra, rb, true, 1);

    /* set initial value = 2 */
    ctd_counters[id].value = 2;

    /* first rising: should decrement to 1 */
    reg[ra] = id;
    reg[rb] = 1;
    op_ctd(instr);
    REPORT("ctd: first rising decremented to 1", ctd_counters[id].value == 1);

    /* Q should be (value <= preset) -> 1 */
    reg[ra] = id;
    reg[rb] = 1;
    op_ctd(instr);
    /* op_ctd writes reg[RA] = (value <= preset) ? 1 : 0; */
    REPORT("ctd: Q true when value <= preset", reg[ra] == 1);

    /* underflow protection: set to 0 and attempt to decrement */
    ctd_counters[id].value = 0;
    vm_counters_reset(); /* also resets prev flags so next rising is observed correctly */
    reg[ra] = id;
    reg[rb] = 1;
    op_ctd(instr); /* rising -> should not make it negative */
    REPORT("ctd: does not underflow below 0", ctd_counters[id].value == 0);
}

static void test_ctud_basic_and_limits(void) {
    vm_counters_reset();

    const uint8_t id = 3;
    const uint8_t ra = 7, rb_up = 8, rc_down = 9;

    uint32_t instr; /* we'll construct different combos */

    /* ensure starting at 0 */
    ctud_counters[id].value = 0;

    /* up rising -> increments */
    reg[ra] = id;
    reg[rb_up] = 1;
    reg[rc_down] = 0;
    instr = make_instr(OPC_CTUD, ra, rb_up, false, rc_down); /* rc used as register index here (not immediate) */
    /* Note: Cv_or_imm uses FIMM flag; to pass register index in C we set imm=false and put rc in low byte */
    instr = ((uint32_t)OPC_CTUD << 25) | ((uint32_t)ra << 17) | ((uint32_t)rb_up << 9) | ((uint32_t)0 << 8) | ((uint32_t)rc_down & 0xFFu);
    op_ctud(instr);
    REPORT("ctud: up rising increments to 1", ctud_counters[id].value == 1 && reg[ra] == 1);

    /* down rising -> decrement */
    vm_counters_reset(); /* reset prev flags so next rising is observed; keep value set manually */
    ctud_counters[id].value = 5;
    reg[ra] = id;
    reg[rb_up] = 0;
    reg[rc_down] = 1;
    instr = ((uint32_t)OPC_CTUD << 25) | ((uint32_t)ra << 17) | ((uint32_t)rb_up << 9) | ((uint32_t)0 << 8) | ((uint32_t)rc_down & 0xFFu);
    op_ctud(instr);
    REPORT("ctud: down rising decremented", ctud_counters[id].value == 4 && reg[ra] == 4);

    /* no underflow below 0 */
    vm_counters_reset();
    ctud_counters[id].value = 0;
    reg[ra] = id;
    reg[rb_up] = 0;
    reg[rc_down] = 1;
    instr = ((uint32_t)OPC_CTUD << 25) | ((uint32_t)ra << 17) | ((uint32_t)rb_up << 9) | ((uint32_t)0 << 8) | ((uint32_t)rc_down & 0xFFu);
    op_ctud(instr);
    REPORT("ctud: down does not underflow below 0", ctud_counters[id].value == 0);

    /* up at UINT32_MAX does not overflow */
    vm_counters_reset();
    ctud_counters[id].value = UINT32_MAX;
    reg[ra] = id;
    reg[rb_up] = 1;
    reg[rc_down] = 0;
    instr = ((uint32_t)OPC_CTUD << 25) | ((uint32_t)ra << 17) | ((uint32_t)rb_up << 9) | ((uint32_t)0 << 8) | ((uint32_t)rc_down & 0xFFu);
    op_ctud(instr);
    REPORT("ctud: up at UINT32_MAX does not overflow", ctud_counters[id].value == UINT32_MAX);
}

int main(void) {
    printf("=== CTU/CTD/CTUD unit tests ===\n");

    test_ctu_basic();
    test_ctu_limits();
    test_ctd_basic_and_limits();
    test_ctud_basic_and_limits();

    printf("================================\n");
    if (fails == 0) {
        printf("ALL COUNTERS TESTS PASSED (%d)\n", tests);
        return 0;
    } else {
        printf("FAILED: %d / %d\n", fails, tests);
        return 1;
    }
}
