#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
//#include "../include/vm32.h"
#include "../include/funcs.h"
#include "../include/cleanup.h"

int main(void) {
    /* prepare mem/reg */
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;
    vm_tables_init();

    // helper build instruction: opcode at top 7 bits etc
    auto make_instr = [](uint8_t opcode, uint8_t ra, uint8_t rb, uint8_t imm, uint8_t c)->uint32_t {
        return ((uint32_t)opcode<<25) | ((uint32_t)ra<<17) | ((uint32_t)rb<<9) | ((uint32_t)imm<<8) | (uint32_t)c;
    };

    const uint8_t OPC_EXIT = /* номер из op_ex, найти в vm32.c — у тебя это после jmp_if_not */ 59; // если 59 — проверь
    // case 1: A==0 -> immediate (C)
    reg[0] = 0;
    uint32_t instr1 = make_instr(OPC_EXIT, 0, 0, 1, 7); // immediate=7
    op_exit(instr1);
    if (vm_exit_code != 7) { printf("FAIL: exit immediate expected 7 got %d\n", vm_exit_code); return 1; }
    printf("PASS: exit immediate\n");

    // case 2: A != 0 -> code from register A (value, not register index)
    reg[1] = 42;
    uint32_t instr2 = make_instr(OPC_EXIT, 1, 0, 1, 7);
    op_exit(instr2);
    if (vm_exit_code != 42) { printf("FAIL: exit from reg expected 42 got %d\n", vm_exit_code); return 1; }
    printf("PASS: exit from reg\n");

    vm_tables_destroy();
    printf("ALL PASS\n");
    return 0;
}
