#include "cleanup.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

atomic_bool vm_stop_requested = ATOMIC_VAR_INIT(false);
int vm_exit_code = 0;

Cleanup *cleanup_list = NULL;

/*
   Register cleanup callback
  */

void vm_register_cleanup(cleanup_fn fn, void *ctx) {
    if (!fn) return;
    Cleanup *n = malloc(sizeof(Cleanup));
    n->fn = fn;
    n->ctx = ctx;
    n->next = cleanup_list;
    cleanup_list = n;
}

/* 
   Run cleanups once
*/

void run_cleanups(void) {
    Cleanup *c = cleanup_list;
    cleanup_list = NULL; // prevent re-entrant use: сбросим голову сразу
    while (c) {
        Cleanup *next = c->next;
        if (c->fn) c->fn(c->ctx);
        free(c);
        c = next;
    }
}

/* 
   External stop request
 */

void vm_request_stop(void) {
    atomic_store(&vm_stop_requested, true);
}
