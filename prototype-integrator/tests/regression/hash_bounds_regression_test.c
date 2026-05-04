#include "test_common.h"
#include "vm_lifecycle.h"
#include "hashing.h"

static int test_memory_hash_bounds_clamped(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->PC_START = VM_MEM_BYTES - 4;
    vm->program_size = 16;

    vm->mem[VM_MEM_BYTES - 4] = 0xDE;
    vm->mem[VM_MEM_BYTES - 3] = 0xAD;
    vm->mem[VM_MEM_BYTES - 2] = 0xBE;
    vm->mem[VM_MEM_BYTES - 1] = 0xEF;

    uint32_t got = vm_calculate_memory_hash_ex(vm, VM_MEM_BYTES - 4, 16, HASH_CRC32);
    uint32_t expected = crc32(vm->mem + (VM_MEM_BYTES - 4), 4);
    ASSERT_EQ_U32(expected, got);

    uint32_t prog_hash = vm_calculate_program_hash_ex(vm, HASH_CRC32);
    ASSERT_EQ_U32(expected, prog_hash);

    vm_destroy(vm);
    return 0;
}

static int test_null_input_returns_zero(void) {
    ASSERT_EQ_U32(0, vm_calculate_memory_hash_ex(NULL, 0, 16, HASH_CRC32));
    ASSERT_EQ_U32(0, vm_calculate_registers_hash_ex(NULL, HASH_CRC32));
    ASSERT_EQ_U32(0, calculate_memory_hash_ex(NULL, 0, 10, HASH_CRC32));
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_memory_hash_bounds_clamped());
    ASSERT_EQ_U32(0, test_null_input_returns_zero());
    printf("[PASS] hash_bounds_regression_test\n");
    return 0;
}
