// main.c
#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "cleanup.h"
#include "timers.h"
#include "vm_tables.h"
#include "hot_reload.h"
#include "vm_helpers.h"
#include "vm_lifecycle.h"
#include "vm_runtime.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static vm_state_t *g_active_vm_for_signal = NULL;

static void handle_sigterm_global(int sig) {
    (void)sig;
    atomic_store(&vm_stop_requested, true);
    if (g_active_vm_for_signal) {
        atomic_store(&g_active_vm_for_signal->stop_requested, true);
    }
}

#ifndef UNIT_TEST
int main(int argc, char **argv) {
    vm_state_t *vm = vm_create();
    if (!vm) {
        fprintf(stderr, "Failed to create VM\n");
        return 1;
    }

    vm_cli_options_t opts;
    int parse_rc = vm_parse_cli(argc, argv, &opts, stderr);
    if (parse_rc != 0) {
        vm_destroy(vm);
        return (parse_rc > 0) ? 0 : 1;
    }

    if (vm_apply_cli_options(vm, &opts, stderr) != 0) {
        vm_destroy(vm);
        return 1;
    }

    if (!vm->program_dir) {
        vm->program_dir = strdup("build/programs");
        if (!vm->program_dir) {
            fprintf(stderr, "Failed to allocate default program_dir\n");
            vm_destroy(vm);
            return 1;
        }
    }

    if (vm_validate_config(vm, stderr) != 0) {
        vm_destroy(vm);
        return 1;
    }

    if (reload_programs_from_directory(vm, vm->program_dir) != 0) {
        fprintf(stderr, "Initial load failed\n");
        vm_destroy(vm);
        return 1;
    }

    directory_changed(vm->program_dir, &vm->program_dir_signature);

    g_active_vm_for_signal = vm;
    signal(SIGINT, handle_sigterm_global);
    signal(SIGTERM, handle_sigterm_global);

    timers_init(vm);
    vm_tables_init(vm);

    run_program(vm);

    vm_tables_destroy(vm);
    vm_destroy(vm);
    return 0;
}
#endif