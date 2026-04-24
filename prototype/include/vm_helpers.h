#ifndef VM_HELPERS_H
#define VM_HELPERS_H

#include "vm32.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *program_dir;

    bool has_clock_rate_hz;
    uint32_t clock_rate_hz;

    bool has_cycle_time_ms;
    uint32_t cycle_time_ms;

    bool set_enable_cycle_check;
    bool enable_cycle_check;

    bool set_enable_hash_check;
    bool enable_hash_check;

    bool set_enable_tick_timing;
    bool enable_tick_timing;

    bool has_hash_algo;
    hash_algorithm_t hash_algo;
} vm_cli_options_t;

void vm_cli_options_init(vm_cli_options_t *opts);

int vm_parse_cli(
    int argc,
    char **argv,
    vm_cli_options_t *opts,
    FILE *err
);

int vm_apply_cli_options(
    vm_state_t *vm,
    const vm_cli_options_t *opts,
    FILE *err
);

int vm_validate_config(
    const vm_state_t *vm,
    FILE *err
);

const char *hash_algorithm_to_string(hash_algorithm_t algo);
void vm_print_config(const vm_state_t *vm, FILE *out);

#endif