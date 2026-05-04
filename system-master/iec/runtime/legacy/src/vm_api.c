#include "vm_api.h"

#include "vm_lifecycle.h"
#include "vm_runtime.h"
#include "vm_helpers.h"
#include "vm_tables.h"
#include "timers.h"
#include "hot_reload.h"

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static vm_state_t *g_vm = NULL;
static vm32_log_cb_t g_log_cb = NULL;

int vm32_init(const vm_config_t *cfg)
{
    if (g_vm) {
        return -1;
    }

    g_vm = vm_create();
    if (!g_vm) {
        return -1;
    }

    if (cfg) {
        g_vm->config = *cfg;
    }

    atomic_store(&g_vm->stop_requested, false);

    timers_init(g_vm);
    vm_tables_init(g_vm);

    return 0;
}

int vm32_load_programs(const char **fnames, int count)
{
    if (!g_vm || !fnames || count <= 0) {
        return -1;
    }

    return vm_load_programs(g_vm, fnames, count);
}

void vm32_execute_cycle(void)
{
    if (!g_vm) {
        return;
    }

    vm_execute_single_cycle(g_vm);
}

void vm32_request_stop(void)
{
    if (!g_vm) {
        return;
    }

    atomic_store(&g_vm->stop_requested, true);
}

int vm32_is_running(void)
{
    if (!g_vm) {
        return 0;
    }

    return !atomic_load(&g_vm->stop_requested);
}

void vm32_shutdown(void)
{
    if (!g_vm) {
        return;
    }

    atomic_store(&g_vm->stop_requested, true);

    vm_tables_destroy(g_vm);
    vm_destroy(g_vm);

    g_vm = NULL;
    g_log_cb = NULL;
}

void vm32_set_log_callback(vm32_log_cb_t cb)
{
    g_log_cb = cb;
}