// hot_reload.h
#ifndef LOADER_H
#define LOADER_H

void apply_pending_reload(vm_state_t *vm);

int vm_schedule_hot_reload(vm_state_t *vm, const char *filename, size_t module_index); 

#endif