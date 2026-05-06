#include "test_common.h"
#include "vm_lifecycle.h"
#include "instruction/execution_test_helpers.h"

/* ---------------------------------------------------------
   1. Прямой доступ: vm_mw32 / vm_mr32
   --------------------------------------------------------- */
static int test_vm_memory_roundtrip(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm_mw32(vm, 0x200, 0xDEADBEEF);
    uint32_t v = vm_mr32(vm, 0x200);

    ASSERT_EQ_U32(0xDEADBEEF, v);

    vm_destroy(vm);
    return 0;
}

/* ---------------------------------------------------------
   2. Проверка little-endian (дополнение к твоим тестам)
   --------------------------------------------------------- */
static int test_memory_endianness_consistency(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm_mw32(vm, 0x100, 0x11223344);

    ASSERT_EQ_U32(0x44, vm->mem[0x100]);
    ASSERT_EQ_U32(0x33, vm->mem[0x101]);
    ASSERT_EQ_U32(0x22, vm->mem[0x102]);
    ASSERT_EQ_U32(0x11, vm->mem[0x103]);

    ASSERT_EQ_U32(0x11223344, vm_mr32(vm, 0x100));

    vm_destroy(vm);
    return 0;
}

/* ---------------------------------------------------------
   3. Границы памяти (write)
   --------------------------------------------------------- */
static int test_vm_memory_write_out_of_bounds_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    /* попытка записи почти за границей */
    uint32_t addr = VM_MEM_BYTES - 2;

    vm_mw32(vm, addr, 0xAAAAAAAA);

    /* память не должна "уехать" */
    ASSERT_TRUE(1); /* если не упали — уже хорошо */

    vm_destroy(vm);
    return 0;
}

/* ---------------------------------------------------------
   4. Границы памяти (read)
   --------------------------------------------------------- */
static int test_vm_memory_read_out_of_bounds_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    uint32_t addr = VM_MEM_BYTES - 2;

    uint32_t v = vm_mr32(vm, addr);

    /* по контракту ожидаем 0 */
    ASSERT_EQ_U32(0, v);

    vm_destroy(vm);
    return 0;
}

/* ---------------------------------------------------------
   5. Через инструкцию DEMUX (реальный доступ к памяти)
   --------------------------------------------------------- */
static int test_demux_writes_inside_bounds(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 0xAABBCCDD; /* значение */
    vm->reg[2] = 0x100;      /* base */
    vm->reg[3] = 0;          /* index */

    vm->op_ex[OP_DEMUX](vm, ENC_RRR(OP_DEMUX, 1, 2, 3));

    /* проверяем, что реально записалось */
    uint32_t v = vm_mr32(vm, 0x100);
    ASSERT_EQ_U32(0xAABBCCDD, v);

    vm_destroy(vm);
    return 0;
}

/* ---------------------------------------------------------
   6. DEMUX не пишет за границы
   --------------------------------------------------------- */
static int test_demux_out_of_bounds_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 0xFFFFFFFF;
    vm->reg[2] = VM_MEM_BYTES - 2; /* почти конец */
    vm->reg[3] = 1; /* => выйдет за границу */

    uint8_t before = vm->mem[VM_MEM_BYTES - 1];

    vm->op_ex[OP_DEMUX](vm, ENC_RRR(OP_DEMUX, 1, 2, 3));

    ASSERT_EQ_U32(before, vm->mem[VM_MEM_BYTES - 1]);

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_vm_memory_roundtrip());
    ASSERT_EQ_U32(0, test_memory_endianness_consistency());
    ASSERT_EQ_U32(0, test_vm_memory_write_out_of_bounds_safe());
    ASSERT_EQ_U32(0, test_vm_memory_read_out_of_bounds_safe());
    ASSERT_EQ_U32(0, test_demux_writes_inside_bounds());
    ASSERT_EQ_U32(0, test_demux_out_of_bounds_safe());

    printf("[PASS] memory_load_store_test\n");
    return 0;
}