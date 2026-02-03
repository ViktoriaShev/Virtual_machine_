#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>

#include "../include/vm32.h"

/*
 Integration test:
  - allocate mem
  - write two instructions at PC_START: NOP; HALT
  - create one module mapping to PC_START with size 8
  - clear vm_stop_requested, call vm_tables_init/timers_init
  - call run_program()
  - check that VM stopped (vm_stop_requested == true) and cycle_count advanced
*/

extern atomic_bool vm_stop_requested; /* defined somewhere in project (cleanup / vm core) */
extern int vm_exit_code;             /* if present, not required */

static inline uint32_t make_instr(uint8_t opcode, uint8_t ra, uint8_t rb, uint8_t imm_flag, uint8_t c) {
    return ((uint32_t)opcode << 25) |
           ((uint32_t)ra << 17) |
           ((uint32_t)rb << 9) |
           ((uint32_t)(imm_flag & 1) << 8) |
           ((uint32_t)c & 0xFFu);
}

int main(void) {
    printf("=== integration: program flow (NOP,HALT) ===\n");

    /* allocate VM memory */
    mem = (uint8_t *)calloc(1, MEM_BYTES);
    if (!mem) {
        perror("calloc(mem)");
        return 2;
    }

    /* zero registers */
    for (int i = 0; i < REG_COUNT; ++i) reg[i] = 0;

    /* ensure globals in known state */
    atomic_store(&vm_stop_requested, false);
    cycle_count = 0;
    module_count = 0;

    /* init timers / tables required by run_program */
    timers_init();
    vm_tables_init();

    /* prepare a single module at PC_START */
    modules = (module_info_t*)calloc(1, sizeof(module_info_t));
    if (!modules) {
        perror("calloc(modules)");
        free(mem);
        return 2;
    }
    modules[0].name = strdup("test_program_flow");
    modules[0].addr = PC_START;
    modules[0].size = 8; /* two 4-byte instructions */
    module_count = 1;

    /* Opcode numbers taken from vm32.c op_ex table:
       HALT = 60, NOP = 61 (see op_ex order in vm32.c)
    */
    const uint8_t OPC_HALT = 60;
    const uint8_t OPC_NOP  = 61;

    /* write NOP at PC_START, HALT at PC_START+4 */
    uint32_t instr_nop  = make_instr(OPC_NOP, 0, 0, 0, 0);
    uint32_t instr_halt = make_instr(OPC_HALT, 0, 0, 0, 0);

    /* helper writes (mw32 inline from vm32.h) */
    mw32(PC_START + 0, instr_nop);
    mw32(PC_START + 4, instr_halt);

    /* Sanity: decode first instruction and check opcode decoded */
    decoded_instr_t *d = vm_decode_instruction(PC_START);
    if (!d) {
        printf("FAIL: decode_instruction returned NULL for NOP\n");
        goto fail_cleanup;
    }
    if (d->opcode != OPC_NOP) {
        printf("FAIL: decoded opcode != expected NOP (got %u, want %u)\n", d->opcode, (unsigned)OPC_NOP);
        goto fail_cleanup;
    } else {
        printf("PASS: decode_instruction(PC_START) -> opcode %u\n", d->opcode);
    }

    /* Run VM: this should execute NOP then HALT which sets vm_stop_requested -> true */
    run_program();

    /* After run_program returns, vm_stop_requested should be true (HALT stores true) */
    if (atomic_load(&vm_stop_requested)) {
        printf("PASS: vm_stop_requested set by HALT\n");
    } else {
        printf("FAIL: vm_stop_requested NOT set after run_program\n");
        goto fail_cleanup;
    }

    if (cycle_count > 0) {
        printf("PASS: cycle_count advanced (%u)\n", cycle_count);
    } else {
        printf("FAIL: cycle_count not advanced\n");
        goto fail_cleanup;
    }

    printf("ALL INTEGRATION TESTS PASSED\n");

    /* cleanup */
    vm_tables_destroy();
    free(modules[0].name);
    free(modules);
    free(mem);
    return 0;

fail_cleanup:
    vm_tables_destroy();
    if (modules) {
        if (modules[0].name) free(modules[0].name);
        free(modules);
    }
    free(mem);
    return 1;
}
