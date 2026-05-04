#define _POSIX_C_SOURCE 200809L

#include "test_common.h"
#include "vm_lifecycle.h"
#include "hot_reload.h"
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

static int test_reload_and_signature_change(void) {
    const char *dir = "build/test_reload_dir";
    mkdir("build", 0755);
    mkdir(dir, 0755);

    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_reload_dir/a.bin", fixture_module_a, sizeof(fixture_module_a)));
    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_reload_dir/b.bin", fixture_module_b, sizeof(fixture_module_b)));

    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    uint32_t sig = 0;
    ASSERT_EQ_U32(0, (uint32_t)directory_changed(dir, &sig));
    ASSERT_TRUE(sig != 0);

    ASSERT_EQ_U32(0, (uint32_t)reload_programs_from_directory(vm, dir));
    uint32_t old_hash = vm->program_hash;

    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_reload_dir/b.bin", fixture_module_b_v2, sizeof(fixture_module_b_v2)));

    ASSERT_EQ_U32(1, (uint32_t)directory_changed(dir, &sig));

    vm->program_dir = strdup(dir);
    ASSERT_NOT_NULL(vm->program_dir);

    atomic_store(&vm->reload_pending, true);
    apply_pending_reload(vm);

    ASSERT_NEQ_U32(old_hash, vm->program_hash);
    ASSERT_FALSE(atomic_load(&vm->reload_pending));

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_reload_and_signature_change());
    printf("[PASS] hot_reload_integration_test\n");
    return 0;
}
