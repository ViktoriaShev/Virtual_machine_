#ifndef VM_TABLES_H
#define VM_TABLES_H

#include <stdint.h>
#include <stdbool.h>

/* tables lifecycle */
void vm_tables_init(void);
void vm_tables_destroy(void);

/* labels */
void labels_add(const char *name, uint32_t addr);
uint32_t *labels_lookup(const char *name);

/* breakpoints (если нужно) */
void bp_set(uint32_t addr);
void bp_clear(uint32_t addr);
bool bp_is_set(uint32_t addr);

#endif
