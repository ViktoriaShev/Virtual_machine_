// testing/test_decode.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../include/vm32.h"
#include "../include/funcs.h"   // если у тебя здесь определены OPC/RA/RB/RC/FIMM/IMM8 макросы

static int tests_run = 0, tests_failed = 0;
static void report(const char *name, int ok) {
    tests_run++;
    if (ok) printf("PASS: %s\n", name);
    else { printf("FAIL: %s\n", name); tests_failed++; }
}

int main(void) {
    printf("=== decode_instruction unit tests ===\n");

    // 1) init environment
    mem = (uint8_t *)calloc(1, MEM_BYTES);
    if (!mem) { perror("calloc mem"); return 2; }
    vm_tables_init(); // создаст decoded_cache и т.д.

    // 2) Simple decode + cache pointer identity
    uint32_t addr = PC_START;
    uint32_t instr = 0x12030405u; // произвольное слово
    mw32(addr, instr);

    decoded_instr_t *d1 = vm_decode_instruction(addr);
    report("decode: returned non-NULL", d1 != NULL);
    if (d1 == NULL) { free(mem); return 1; }

    decoded_instr_t *d2 = vm_decode_instruction(addr);
    report("decode: cache returns same pointer on repeated call", d1 == d2);

    // 3) fields match macros (use macros defined в проекте)
    report("decode: raw_instr preserved", d1->raw_instr == instr);
    report("decode: opcode matches OPC()", d1->opcode == OPC(instr));
    report("decode: ra matches RA()", d1->ra == RA(instr));
    report("decode: rb matches RB()", d1->rb == RB(instr));
    report("decode: rc matches RC()", d1->rc == RC(instr));
    report("decode: has_immediate matches FIMM()", d1->has_immediate == (bool)FIMM(instr));
    report("decode: immediate matches IMM8()", d1->immediate == (d1->has_immediate ? IMM8(instr) : 0));

    // 4) bounds check: address too close to end -> should return NULL
    decoded_instr_t *d_out = vm_decode_instruction((uint32_t)(MEM_BYTES - 2));
    report("decode: out-of-range address returns NULL", d_out == NULL);

    // 5) invalid opcode handling: put an opcode that has no handler
    uint8_t bad_opcode = (uint8_t)0xFF; // very likely out of OPCODE_COUNT
    uint32_t bad_instr = ((uint32_t)bad_opcode << 24) | 0x00112233u;
    mw32(addr + 4, bad_instr);
    decoded_instr_t *db = vm_decode_instruction(addr + 4);
    report("decode: invalid opcode decoded (non-NULL)", db != NULL);
    if (db) {
        int has_handler = (db->opcode < OPCODE_COUNT && op_ex[db->opcode] != NULL);
        report("decode: invalid opcode has no handler (expected)", has_handler == 0);
    }

    // 6) no-immediate instruction must have immediate == 0
    uint32_t instr_noimm = instr & ~(1u << 23); // сброс FIMM
    mw32(addr + 8, instr_noimm);
    decoded_instr_t *d_noimm = vm_decode_instruction(addr + 8);

    report("decode: no-immediate sets immediate == 0",
        d_noimm && d_noimm->has_immediate == false && d_noimm->immediate == 0);
    
    // 7) zero instruction should decode safely
    mw32(addr + 12, 0x00000000u);
    decoded_instr_t *dz = vm_decode_instruction(addr + 12);

    report("decode: zero instruction decoded (non-NULL)", dz != NULL);
    if (dz) {
        report("decode: zero instr opcode == 0", dz->opcode == 0);
        report("decode: zero instr has no immediate", dz->has_immediate == false);
    }

        // 8) different addresses produce different cache entries
    mw32(addr + 16, instr ^ 0x01020304u);
    decoded_instr_t *d_other = vm_decode_instruction(addr + 16);

    report("decode: different addr yields different decoded ptr",
        d_other && d_other != d1);

    // 9) decode cache stress (many instructions)
    int stress_ok = 1;
    for (uint32_t off = 0; off < 256; off += 4) {
        uint32_t a = addr + 0x100 + off;
        mw32(a, 0xAABB0000u | off);
        decoded_instr_t *ds = vm_decode_instruction(a);
        if (!ds) { stress_ok = 0; break; }
    }
    report("decode: cache stress (256 instr)", stress_ok);

    // cleanup
    vm_tables_destroy();
    free(mem);

    if (tests_failed == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d of %d\n", tests_failed, tests_run);
        return 1;
    }
}
