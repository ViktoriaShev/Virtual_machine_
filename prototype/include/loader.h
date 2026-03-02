// loader.h
#ifndef LOADER_H
#define LOADER_H

#include "vm32.h"

/* Загружает бинарники в память VM.
   Заполняет vm->modules, vm->module_count, vm->program_hash, vm->program_size.
   Возвращает 0 при успехе, <0 при ошибке. */
int load_programs(vm_state_t *vm, const char **fnames, int count);

#endif /* LOADER_H */
