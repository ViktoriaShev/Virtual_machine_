#ifndef VM_TABLES_H
#define VM_TABLES_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

typedef struct {
    uint32_t raw_instr;

    uint8_t opcode;
    uint8_t ra, rb, rc;

    uint32_t immediate;
    bool has_immediate;
} decoded_instr_t;

/* tables lifecycle */
int vm_tables_init(vm_state_t *vm);
void vm_tables_destroy(vm_state_t *vm);

/* labels */
void labels_add(vm_state_t *vm, const char *name, uint32_t addr);
uint32_t *labels_lookup(vm_state_t *vm, const char *name);

/* breakpoints (если нужно) */
void bp_set(vm_state_t *vm, uint32_t addr);
void bp_clear(vm_state_t *vm, uint32_t addr) ;

bool bp_is_set(vm_state_t *vm, uint32_t addr) ;

decoded_instr_t *vm_decode_instruction(vm_state_t *vm, uint32_t addr);

#endif
