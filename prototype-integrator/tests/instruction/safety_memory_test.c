#include "test_common.h"
#include "vm_lifecycle.h"
#include "instruction/execution_test_helpers.h"

static int test_invalid_opcode_stops_execution(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    const uint32_t program[] = {
        ENC_RRR(100, 1, 2, 3) /* outside implemented opcode table */
    };

    load_program_words(vm, 0x2200, program, 1);

    ASSERT_EQ_U32((uint32_t)-1, (uint32_t)vm_step_for_test(vm, 0x2200, 0x2204));
    ASSERT_FALSE(vm->running);

    vm_destroy(vm);
    return 0;
}

static int test_demux_out_of_bounds_is_ignored(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[7] = 0xAABBCCDD;
    vm->reg[8] = (uint32_t)(VM_MEM_BYTES - 2); /* base */
    vm->reg[9] = 1;                            /* idx => addr + 4 out of bounds */

    uint8_t before0 = vm->mem[VM_MEM_BYTES - 1];
    uint8_t before1 = vm->mem[VM_MEM_BYTES - 2];

    vm->op_ex[OP_DEMUX](vm, ENC_RRR(OP_DEMUX, 7, 8, 9)); /* DEMUX */

    ASSERT_EQ_U32(before0, vm->mem[VM_MEM_BYTES - 1]);
    ASSERT_EQ_U32(before1, vm->mem[VM_MEM_BYTES - 2]);

    vm_destroy(vm);
    return 0;
}

static int test_vm_memory_little_endian_roundtrip(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm_mw32(vm, 0x100, 0x11223344u);

    ASSERT_EQ_U32(0x44, vm->mem[0x100]);
    ASSERT_EQ_U32(0x33, vm->mem[0x101]);
    ASSERT_EQ_U32(0x22, vm->mem[0x102]);
    ASSERT_EQ_U32(0x11, vm->mem[0x103]);
    ASSERT_EQ_U32(0x11223344u, vm_mr32(vm, 0x100));

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_invalid_opcode_stops_execution());
    ASSERT_EQ_U32(0, test_demux_out_of_bounds_is_ignored());
    ASSERT_EQ_U32(0, test_vm_memory_little_endian_roundtrip());
    printf("[PASS] instruction/safety_memory_test\n");
    return 0;
}
