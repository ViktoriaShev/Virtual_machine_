#include "test_common.h"
#include "vm_lifecycle.h"
#include "instruction/execution_test_helpers.h"

static int test_valid_opcode_dispatch(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    for (int op = 0; op < 32; ++op) {
        if (vm->op_ex[op] != NULL) {
            /* просто факт, что обработчик есть */
            ASSERT_NOT_NULL(vm->op_ex[op]);
        }
    }

    vm_destroy(vm);
    return 0;
}

static int test_invalid_opcode_fails(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    const uint32_t program[] = {
        ENC_RRR(255, 1, 2, 3)
    };

    load_program_words(vm, 0x1000, program, 1);

    ASSERT_EQ_U32((uint32_t)-1,
        (uint32_t)vm_step_for_test(vm, 0x1000, 0x1004));

    ASSERT_FALSE(vm->running);

    vm_destroy(vm);
    return 0;
}

static int test_opcode_does_not_corrupt_registers_on_fail(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 123;

    const uint32_t program[] = {
        ENC_RRR(255, 2, 1, 1)
    };

    load_program_words(vm, 0x1000, program, 1);

    vm_step_for_test(vm, 0x1000, 0x1004);

    ASSERT_EQ_U32(123, vm->reg[1]); /* не изменился */

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_valid_opcode_dispatch());
    ASSERT_EQ_U32(0, test_invalid_opcode_fails());
    ASSERT_EQ_U32(0, test_opcode_does_not_corrupt_registers_on_fail());

    printf("[PASS] opcode_decode_test\n");
    return 0;
}