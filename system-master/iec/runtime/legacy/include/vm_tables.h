#ifndef VM_TABLES_H
#define VM_TABLES_H

#include <stdint.h>
#include <stdbool.h>

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

#endif
