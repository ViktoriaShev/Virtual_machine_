#include "test_common.h"
#include "vm_lifecycle.h"
#include "timers.h"

static int test_ton_basic_delay(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->time_ms = 1000;
    ton_set(vm, 0, true, 100);
    ASSERT_FALSE(ton_Q(vm, 0));

    vm->time_ms = 1090;
    ton_set(vm, 0, true, 100);
    ASSERT_FALSE(ton_Q(vm, 0));

    vm->time_ms = 1100;
    ton_set(vm, 0, true, 100);
    ASSERT_TRUE(ton_Q(vm, 0));

    vm->time_ms = 1110;
    ton_set(vm, 0, false, 100);
    ASSERT_FALSE(ton_Q(vm, 0));

    vm_destroy(vm);
    return 0;
}

static int test_tp_generates_pulse(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->time_ms = 2000;
    tp_set(vm, 1, true, 50);
    ASSERT_TRUE(tp_Q(vm, 1));

    vm->time_ms = 2020;
    tp_set(vm, 1, true, 50);
    ASSERT_TRUE(tp_Q(vm, 1));

    vm->time_ms = 2051;
    tp_set(vm, 1, true, 50);
    ASSERT_FALSE(tp_Q(vm, 1));

    vm_destroy(vm);
    return 0;
}

static int test_invalid_id_is_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->time_ms = 3000;
    ton_set(vm, 99, true, 1);
    ASSERT_FALSE(ton_Q(vm, 99));

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_ton_basic_delay());
    ASSERT_EQ_U32(0, test_tp_generates_pulse());
    ASSERT_EQ_U32(0, test_invalid_id_is_safe());

    printf("[PASS] timers_unit_test\n");
    return 0;
}
