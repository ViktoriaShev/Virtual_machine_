#ifndef VM_RUNTIME_H
#define VM_RUNTIME_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

int run_program(vm_state_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* VM_RUNTIME_H */