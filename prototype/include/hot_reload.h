#ifndef HOT_RELOAD_H
#define HOT_RELOAD_H

#include <stdint.h>
#include <stddef.h>

typedef struct vm_state vm_state_t;

int collect_bin_files(const char *dir, char ***out_files, int *out_count);

int directory_changed(const char *dir, uint32_t *prev_sig);

int reload_programs_from_directory(vm_state_t *vm, const char *dir);

void apply_pending_reload(vm_state_t *vm);

#endif