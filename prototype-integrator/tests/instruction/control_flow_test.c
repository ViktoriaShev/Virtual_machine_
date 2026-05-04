#include "test_common.h"
#include "vm_lifecycle.h"
#include "hashing.h"
#include "instruction/execution_test_helpers.h"

static int run_deterministic_program(uint32_t *out_hash, uint32_t *out_steps) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    const uint32_t base = 0x3000;
    const uint32_t halt_addr = base + 16;

    vm->reg[20] = halt_addr; /* target for JMP_IF */
    vm->reg[21] = 1;         /* condition */

    const uint32_t program[] = {
        ENC_RRI(OP_ADD, 5, 0, 7),       /* R5 = R0 + 7 -> 12 */
        ENC_RRR(OP_JMP_IF, 0, 20, 21),  /* JMP_IF R20 if R21 != 0 */
        ENC_RRI(OP_ADD, 6, 0, 99),      /* should be skipped */
        ENC_RRR(OP_NOP, 0, 0, 0),       /* NOP (also skipped) */
        ENC_RRR(OP_HALT, 0, 0, 0)       /* HALT */
    };

    load_program_words(vm, base, program, sizeof(program) / sizeof(program[0]));

    uint32_t steps = 0;
    ASSERT_EQ_U32(0, (uint32_t)vm_run_for_test(vm, base, sizeof(program) / sizeof(program[0]), 16, &steps));

    ASSERT_FALSE(vm->running);
    ASSERT_EQ_U32(3, steps);        /* ADD + JMP_IF + HALT */
    ASSERT_EQ_U32(12, vm->reg[5]);
    ASSERT_EQ_U32(0, vm->reg[6]);

    *out_hash = calculate_registers_hash_ex(vm->reg, REG_COUNT, HASH_CRC32);
    *out_steps = steps;

    vm_destroy(vm);
    return 0;
}

static int test_control_flow_and_halt_step_count(void) {
    uint32_t hash1 = 0;
    uint32_t steps1 = 0;

    ASSERT_EQ_U32(0, run_deterministic_program(&hash1, &steps1));
    ASSERT_EQ_U32(3, steps1);
    ASSERT_NEQ_U32(0, hash1);
    return 0;
}

static int test_deterministic_repeatability(void) {
    uint32_t hash1 = 0, hash2 = 0;
    uint32_t steps1 = 0, steps2 = 0;

    ASSERT_EQ_U32(0, run_deterministic_program(&hash1, &steps1));
    ASSERT_EQ_U32(0, run_deterministic_program(&hash2, &steps2));

    ASSERT_EQ_U32(steps1, steps2);
    ASSERT_EQ_U32(hash1, hash2);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_control_flow_and_halt_step_count());
    ASSERT_EQ_U32(0, test_deterministic_repeatability());
    printf("[PASS] instruction/control_flow_test\n");
    return 0;
}
