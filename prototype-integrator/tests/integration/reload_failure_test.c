#define _POSIX_C_SOURCE 200809L

#include "test_common.h"
#include "hot_reload.h"
#include "loader.h"
#include "vm_lifecycle.h"
#include "fixtures/fixture_programs.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static int write_bytes(const char *path, const uint8_t *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size) ? 0 : -1;
}

static int test_reload_failure_keeps_previous_state(void) {
    const char *ok_dir = "build/test_reload_failure_ok";
    const char *bad_dir = "build/test_reload_failure_empty";

    mkdir("build", 0755);
    mkdir(ok_dir, 0755);
    mkdir(bad_dir, 0755);

    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_reload_failure_ok/module_a.bin",
                                           fixture_module_a, sizeof(fixture_module_a)));

    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    const char *files[] = {
        "build/test_reload_failure_ok/module_a.bin"
    };

    ASSERT_EQ_U32(0, (uint32_t)load_programs(vm, files, 1));

    uint32_t old_hash = vm->program_hash;
    size_t old_module_count = vm->module_count;
    uint32_t old_size = vm->program_size;

    free(vm->program_dir);
    vm->program_dir = strdup(bad_dir);
    ASSERT_NOT_NULL(vm->program_dir);

    atomic_store(&vm->reload_pending, true);
    apply_pending_reload(vm);

    ASSERT_TRUE(atomic_load(&vm->reload_pending));
    ASSERT_EQ_U32(old_hash, vm->program_hash);
    ASSERT_EQ_U32((uint32_t)old_module_count, (uint32_t)vm->module_count);
    ASSERT_EQ_U32(old_size, vm->program_size);

    ASSERT_EQ_U32(vm->PC_START, vm->modules[0].addr);
    ASSERT_EQ_U32(8, vm->modules[0].size);
    ASSERT_MEM_EQ(fixture_module_a, vm->mem + vm->modules[0].addr, sizeof(fixture_module_a));

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_reload_failure_keeps_previous_state());
    printf("[PASS] integration/reload_failure_test\n");
    return 0;
}