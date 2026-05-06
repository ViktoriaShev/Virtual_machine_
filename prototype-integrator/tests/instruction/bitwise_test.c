#include "test_common.h"
#include "vm_lifecycle.h"
#include "instruction/execution_test_helpers.h"

static int test_and_or_xor(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 0xF0F0F0F0u;
    vm->reg[2] = 0x0FF00FF0u;

    const uint32_t program[] = {
        ENC_RRR(OP_AND, 3, 1, 2),
        ENC_RRR(OP_OR,  4, 1, 2),
        ENC_RRR(OP_XOR, 5, 1, 2),
        ENC_RRR(OP_HALT, 0, 0, 0)
    };

    load_program_words(vm, 0x3000, program, 4);

    uint32_t steps = 0;
    ASSERT_EQ_U32(0, (uint32_t)vm_run_for_test(vm, 0x3000, 4, 16, &steps));

    ASSERT_EQ_U32(4, steps);
    ASSERT_EQ_U32(0x00F000F0u, vm->reg[3]);
    ASSERT_EQ_U32(0xFFF0FFF0u, vm->reg[4]);
    ASSERT_EQ_U32(0xFF00FF00u, vm->reg[5]);

    vm_destroy(vm);
    return 0;
}

static int test_not_operation(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 0;

    const uint32_t program[] = {
        ENC_RRR(OP_NOT, 2, 1, 0),
        ENC_RRR(OP_HALT, 0, 0, 0)
    };

    load_program_words(vm, 0x3000, program, 2);

    uint32_t steps = 0;
    ASSERT_EQ_U32(0, (uint32_t)vm_run_for_test(vm, 0x3000, 2, 16, &steps));

    ASSERT_EQ_U32(2, steps);
    ASSERT_EQ_U32(0xFFFFFFFFu, vm->reg[2]);

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_and_or_xor());
    ASSERT_EQ_U32(0, test_not_operation());

    printf("[PASS] bitwise_test\n");
    return 0;
}