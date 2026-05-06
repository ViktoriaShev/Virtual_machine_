#define _POSIX_C_SOURCE 200809L

#include "test_common.h"
#include "hot_reload.h"
#include "loader.h"
#include "hashing.h"
#include "vm_lifecycle.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static int write_bytes(const char *path, const uint8_t *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size) ? 0 : -1;
}

static void free_file_list(char **files, int count) {
    if (!files) return;
    for (int i = 0; i < count; ++i) {
        free(files[i]);
    }
    free(files);
}

static int test_collect_and_map_modules(void) {
    const char *dir = "build/test_io_module_mapping_dir";
    mkdir("build", 0755);
    mkdir(dir, 0755);

    static const uint8_t a_bin[] = {
        0x11, 0x11, 0x11, 0x11,
        0x22, 0x22, 0x22, 0x22
    };
    static const uint8_t m_bin[] = {
        0x33, 0x33, 0x33, 0x33,
        0x44, 0x44, 0x44, 0x44
    };
    static const uint8_t z_bin[] = {
        0x55, 0x55, 0x55, 0x55,
        0x66, 0x66, 0x66, 0x66
    };
    static const uint8_t txt_bin[] = {
        0xAA, 0xBB, 0xCC, 0xDD
    };

    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_io_module_mapping_dir/z.bin", z_bin, sizeof(z_bin)));
    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_io_module_mapping_dir/a.bin", a_bin, sizeof(a_bin)));
    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_io_module_mapping_dir/m.bin", m_bin, sizeof(m_bin)));
    ASSERT_EQ_U32(0, (uint32_t)write_bytes("build/test_io_module_mapping_dir/ignore.txt", txt_bin, sizeof(txt_bin)));

    mkdir("build/test_io_module_mapping_dir/subdir.bin", 0755);

    char **files = NULL;
    int count = 0;

    ASSERT_EQ_U32(0, (uint32_t)collect_bin_files(dir, &files, &count));
    ASSERT_EQ_U32(3, (uint32_t)count);

    ASSERT_TRUE(strstr(files[0], "a.bin") != NULL);
    ASSERT_TRUE(strstr(files[1], "m.bin") != NULL);
    ASSERT_TRUE(strstr(files[2], "z.bin") != NULL);

    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    ASSERT_EQ_U32(0, (uint32_t)load_programs(vm, (const char **)files, count));

    ASSERT_EQ_U32(3, (uint32_t)vm->module_count);
    ASSERT_EQ_U32(24, (uint32_t)vm->program_size);

    ASSERT_EQ_U32(vm->PC_START, vm->modules[0].addr);
    ASSERT_EQ_U32(vm->PC_START + 8, vm->modules[1].addr);
    ASSERT_EQ_U32(vm->PC_START + 16, vm->modules[2].addr);

    ASSERT_EQ_U32(8, vm->modules[0].size);
    ASSERT_EQ_U32(8, vm->modules[1].size);
    ASSERT_EQ_U32(8, vm->modules[2].size);

    ASSERT_MEM_EQ(a_bin, vm->mem + vm->modules[0].addr, sizeof(a_bin));
    ASSERT_MEM_EQ(m_bin, vm->mem + vm->modules[1].addr, sizeof(m_bin));
    ASSERT_MEM_EQ(z_bin, vm->mem + vm->modules[2].addr, sizeof(z_bin));

    uint32_t expected_hash = calculate_memory_hash(vm->mem, vm->PC_START, vm->program_size);
    ASSERT_EQ_U32(expected_hash, vm->program_hash);

    vm_destroy(vm);
    free_file_list(files, count);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_collect_and_map_modules());
    printf("[PASS] integration/io_module_mapping_test\n");
    return 0;
}