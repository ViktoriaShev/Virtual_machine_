// loader.c
#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "hashing.h"   /* calculate_memory_hash(vm->mem, ...) */
#include "debug.h"     /* opcode_name() если нужно */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int load_programs(vm_state_t *vm, const char **fnames, int count) {
    if (!vm || count <= 0) {
        fprintf(stderr, "load_programs: invalid args\n");
        return -1;
    }

    /* освобождаем старые модули если были */
    if (vm->modules) {
        for (size_t i = 0; i < vm->module_count; ++i) free(vm->modules[i].name);
        free(vm->modules);
        vm->modules = NULL;
        vm->module_count = 0;
    }

    vm->modules = (module_info_t *)calloc((size_t)count, sizeof(module_info_t));
    if (!vm->modules) {
        fprintf(stderr, "Failed to allocate modules table\n");
        return -1;
    }

    uint32_t write_ptr = vm->PC_START;
    vm->program_size = 0;
    vm->program_hash = 0;
    vm->module_count = 0;

    for (int i = 0; i < count; ++i) {
        const char *fname = fnames[i];
        FILE *fp = fopen(fname, "rb");
        if (!fp) {
            fprintf(stderr, "Failed to open program file %s\n", fname);
            /* cleanup partial */
            for (size_t j = 0; j < vm->module_count; ++j) free(vm->modules[j].name);
            free(vm->modules);
            vm->modules = NULL;
            vm->module_count = 0;
            return -1;
        }

        if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
        long fsize = ftell(fp);
        rewind(fp);

        if (fsize == 0) {
            fprintf(stderr, "Warning: file %s is empty (size=0)\n", fname);
            vm->modules[vm->module_count].name = strdup(fname);
            vm->modules[vm->module_count].addr = write_ptr;
            vm->modules[vm->module_count].size = 0;
            vm->module_count++;
            fclose(fp);
            continue;
        }

        if ((uint64_t)write_ptr + (uint64_t)fsize > (uint64_t)VM_MEM_BYTES) {
            fprintf(stderr, "Error: loading %s would overflow VM memory\n", fname);
            fclose(fp);
            for (size_t j = 0; j < vm->module_count; ++j) free(vm->modules[j].name);
            free(vm->modules);
            vm->modules = NULL;
            vm->module_count = 0;
            return -1;
        }

        size_t bytes_read = fread(vm->mem + write_ptr, 1, (size_t)fsize, fp);
        if (bytes_read != (size_t)fsize) {
            fprintf(stderr, "Error reading file %s: read %zu of %ld\n", fname, bytes_read, fsize);
            fclose(fp);
            for (size_t j = 0; j < vm->module_count; ++j) free(vm->modules[j].name);
            free(vm->modules);
            vm->modules = NULL;
            vm->module_count = 0;
            return -1;
        }

        vm->modules[vm->module_count].name = strdup(fname);
        vm->modules[vm->module_count].addr = write_ptr;
        vm->modules[vm->module_count].size = (uint32_t)bytes_read;
        vm->module_count++;

        printf("Loaded %zu bytes from %s into memory at 0x%08X\n", bytes_read, fname, write_ptr);

        write_ptr += (uint32_t)bytes_read;
        vm->program_size += (uint32_t)bytes_read;

        fclose(fp);
    }

    if (vm->module_count == 0) {
        fprintf(stderr, "ERROR: No modules were loaded successfully\n");
        free(vm->modules);
        vm->modules = NULL;
        return -1;
    }

    vm->program_hash = calculate_memory_hash(vm->mem, vm->PC_START, vm->program_size);

    printf("\n=== Module Map ===\n");
    for (size_t i = 0; i < vm->module_count; ++i) {
        printf("Module %zu: %s\n", i, vm->modules[i].name);
        printf("  Address: 0x%08X\n", vm->modules[i].addr);
        printf("  Size:    %u bytes\n", vm->modules[i].size);
    }
    printf("==================\n\n");

    printf("Combined program size: %zu bytes\n", vm->program_size);
    printf("Combined program hash (CRC32): 0x%08X\n", vm->program_hash);

    /* debug: печать первых трёх инструкций каждого модуля */
    for (size_t mi = 0; mi < vm->module_count; ++mi) {
        uint32_t base = vm->modules[mi].addr;
        uint32_t sz = vm->modules[mi].size;
        printf("Module %zu: %s at 0x%08X (%u bytes)\n", mi, vm->modules[mi].name, base, (unsigned)sz);
        for (size_t i = 0; i < 3 && (i * 4) < sz; ++i) {
            uint32_t addr = base + (uint32_t)(i * 4);
            uint32_t instr = vm_mr32(vm, addr);
#ifdef HAVE_OPCODE_NAME
            printf("  [%s + 0x%04X] 0x%08X - %s\n",
                   vm->modules[mi].name, (unsigned int)(i * 4), instr,
                   opcode_name(OPC(instr)));
#else
            printf("  [%s + 0x%04X] 0x%08X\n",
                   vm->modules[mi].name, (unsigned int)(i * 4), instr);
#endif
        }
    }

    return 0;
}
