#define _POSIX_C_SOURCE 200809L

#include "test_common.h"
#include "vm_lifecycle.h"
#include "loader.h"
#include "hashing.h"
#include "fixtures/fixture_programs.h"

#include <sys/stat.h>
#include <sys/types.h>

static int write_bytes(const char *path, const uint8_t *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size) ? 0 : -1;
}

static int test_load_two_modules_and_hash(void) {
    const char *dir = "build/test_loader_dir";
    mkdir("build", 0755);
    mkdir(dir, 0755);

    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_loader_dir/module_a.bin", fixture_module_a, sizeof(fixture_module_a)));
    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_loader_dir/module_b.bin", fixture_module_b, sizeof(fixture_module_b)));

    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    const char *files[] = {
        "build/test_loader_dir/module_a.bin",
        "build/test_loader_dir/module_b.bin"
    };

    ASSERT_EQ_U32(0, (uint32_t)load_programs(vm, files, 2));
    ASSERT_EQ_U32(2, (uint32_t)vm->module_count);
    ASSERT_EQ_U32(16, (uint32_t)vm->program_size);

    ASSERT_EQ_U32(vm->PC_START, vm->modules[0].addr);
    ASSERT_EQ_U32(vm->PC_START + 8, vm->modules[1].addr);

    ASSERT_MEM_EQ(fixture_module_a, vm->mem + vm->PC_START, sizeof(fixture_module_a));
    ASSERT_MEM_EQ(fixture_module_b, vm->mem + vm->PC_START + sizeof(fixture_module_a), sizeof(fixture_module_b));

    uint32_t expected_hash = calculate_memory_hash(vm->mem, vm->PC_START, vm->program_size);
    ASSERT_EQ_U32(expected_hash, vm->program_hash);

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_load_two_modules_and_hash());
    printf("[PASS] loader_integration_test\n");
    return 0;
}
