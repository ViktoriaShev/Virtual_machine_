#ifndef CLEANUP_H
#define CLEANUP_H

#include <stdatomic.h>

// graceful shutdown flag
extern atomic_bool vm_stop_requested;

// exit code
extern int vm_exit_code;

// Cleanup hook
typedef void (*cleanup_fn)(void*);

typedef struct Cleanup {
    cleanup_fn fn;
    void *ctx;
    struct Cleanup *next;
} Cleanup;

// глобальный список cleanup-функций
extern Cleanup *cleanup_list;

// API
void vm_register_cleanup(cleanup_fn fn, void *ctx);

void run_cleanups(void);

#endif
