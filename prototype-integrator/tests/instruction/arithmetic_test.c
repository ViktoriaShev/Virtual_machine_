#include "test_common.h"
#include "vm_lifecycle.h"
#include "instruction/execution_test_helpers.h"

static int test_arithmetic_and_div_by_zero(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 12;
    vm->reg[2] = 5;

    const uint32_t program[] = {
        ENC_RRR(OP_ADD, 10, 1, 2), /* ADD R10, R1, R2 = 17 */
        ENC_RRI(OP_SUB, 11, 1, 2), /* SUB R11, R1, #2 = 10 */
        ENC_RRR(OP_MUL, 12, 1, 2), /* MUL R12, R1, R2 = 60 */
        ENC_RRI(OP_DIV, 13, 1, 3), /* DIV R13, R1, #3 = 4 */
        ENC_RRI(OP_DIV, 14, 1, 0), /* DIV R14, R1, #0 = 0 (safe path) */
        ENC_RRR(OP_NOP, 0, 0, 0)   /* NOP */
    };

    load_program_words(vm, 0x2000, program, sizeof(program) / sizeof(program[0]));

    uint32_t steps = 0;
    ASSERT_EQ_U32(0, (uint32_t)vm_run_for_test(vm, 0x2000, sizeof(program) / sizeof(program[0]), 32, &steps));

    ASSERT_EQ_U32(6, steps);
    ASSERT_EQ_U32(17, vm->reg[10]);
    ASSERT_EQ_U32(10, vm->reg[11]);
    ASSERT_EQ_U32(60, vm->reg[12]);
    ASSERT_EQ_U32(4, vm->reg[13]);
    ASSERT_EQ_U32(0, vm->reg[14]);

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_arithmetic_and_div_by_zero());
    printf("[PASS] instruction/arithmetic_test\n");
    return 0;
}
