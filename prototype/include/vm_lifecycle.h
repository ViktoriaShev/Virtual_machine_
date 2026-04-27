#ifndef VM_LIFECYCLE_H
#define VM_LIFECYCLE_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

vm_state_t *vm_create(void);
void vm_destroy(vm_state_t *vm);
int vm_init_defaults(vm_state_t *vm);
int vm_reset(vm_state_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* VM_LIFECYCLE_H */