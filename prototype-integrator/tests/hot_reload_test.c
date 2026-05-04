#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "main.h"
#include "vm_lifecycle.h"
#include "vm_runtime.h"
#include "loader.h"

static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t running = 1;

static const char *current = NULL;
static const char *v1 = "build/bin/program_v1.bin";
static const char *v2 = "build/bin/program_v2.bin";

static void sigusr1(int sig) {
    (void)sig;
    reload_requested = 1;
}

static void sigint(int sig) {
    (void)sig;
    running = 0;
}

static void sleep_ms(int ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };
    nanosleep(&ts, NULL);
}

static int load(vm_state_t *vm, const char *file) {
    const char *files[] = { file };

    if (load_programs(vm, files, 1) != 0) {
        fprintf(stderr, "load failed: %s\n", file);
        return -1;
    }

    vm->PC = vm->PC_START;
    atomic_store(&vm->stop_requested, false);

    printf("[LOAD] %s\n", file);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2) v1 = argv[1];
    if (argc >= 3) v2 = argv[2];

    signal(SIGUSR1, sigusr1);
    signal(SIGINT, sigint);
    signal(SIGTERM, sigint);

    vm_state_t *vm = vm_create();
    if (!vm) return 1;

    vm_init_defaults(vm);
    vm->config.cycle_time_ms = 100;

    current = v1;

    if (load(vm, current) != 0) {
        vm_destroy(vm);
        return 1;
    }

    unsigned cycle = 0;

    while (running) {
        cycle++;

        printf("\n=== TEST CYCLE %u ===\n", cycle);
        printf("PC = 0x%08X | R1 = %u\n", vm->PC, vm->reg[1]);

        /* ВАЖНО: один шаг VM, НЕ run_program() */
        run_program(vm);   // если он у тебя "1 цикл" — ок
                          // если бесконечный — надо заменить (ниже объясню)

        if (reload_requested) {
            reload_requested = 0;

            current = (strcmp(current, v1) == 0) ? v2 : v1;
            load(vm, current);
        }

        sleep_ms(vm->config.cycle_time_ms);
    }

    vm_destroy(vm);
    return 0;
}