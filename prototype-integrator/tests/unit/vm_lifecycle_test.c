#include "test_common.h"
#include "vm_lifecycle.h"

#include <stdatomic.h>

static int test_create_and_destroy(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    ASSERT_NOT_NULL(vm->mem);
    ASSERT_TRUE(vm->running);
    ASSERT_EQ_U32(0, vm->PC);
    ASSERT_FALSE(atomic_load(&vm->stop_requested));
    ASSERT_FALSE(atomic_load(&vm->reload_pending));

    vm_destroy(vm);
    return 0;
}

static int test_init_defaults(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    /* Сначала испортим состояние, чтобы увидеть сброс */
    vm->running = false;
    vm->PC = 0x1234;
    vm->config.cycle_time_ms = 0;
    vm->reg[0] = 999;
    vm->reg[1] = 888;

    ASSERT_EQ_U32(0, (uint32_t)vm_init_defaults(vm));

    ASSERT_TRUE(vm->running);
    ASSERT_EQ_U32(0, vm->PC);
    ASSERT_TRUE(vm->config.cycle_time_ms > 0);
    ASSERT_EQ_U32(5, vm->reg[0]);
    ASSERT_EQ_U32(3, vm->reg[1]);

    vm_destroy(vm);
    return 0;
}

static int test_reset_runtime(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->running = false;
    vm->PC = 0x5555;
    vm->time_ms = 77;
    vm->exit_code = 9;
    vm->reg[0] = 100;
    vm->reg[1] = 200;
    atomic_store(&vm->stop_requested, true);
    atomic_store(&vm->reload_pending, true);

    ASSERT_EQ_U32(0, (uint32_t)vm_reset(vm));

    ASSERT_TRUE(vm->running);
    ASSERT_EQ_U32(vm->PC_START, vm->PC);
    ASSERT_EQ_U32(0, vm->time_ms);
    ASSERT_EQ_U32(0, vm->exit_code);
    ASSERT_FALSE(atomic_load(&vm->stop_requested));
    ASSERT_FALSE(atomic_load(&vm->reload_pending));
    ASSERT_EQ_U32(5, vm->reg[0]);
    ASSERT_EQ_U32(3, vm->reg[1]);

    vm_destroy(vm);
    return 0;
}

static int test_start_stop_flags(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    ASSERT_TRUE(vm->running);
    vm->running = false;
    ASSERT_FALSE(vm->running);
    vm->running = true;
    ASSERT_TRUE(vm->running);

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_create_and_destroy());
    ASSERT_EQ_U32(0, test_init_defaults());
    ASSERT_EQ_U32(0, test_reset_runtime());
    ASSERT_EQ_U32(0, test_start_stop_flags());

    printf("[PASS] vm_lifecycle_test\n");
    return 0;
}