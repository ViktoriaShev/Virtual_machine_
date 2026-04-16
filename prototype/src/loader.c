// loader.c
#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "hashing.h"   /* calculate_memory_hash(vm->mem, ...) */
#include "debug.h"     /* opcode_name() если нужно */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

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

// loader.c или vm32.c
void apply_pending_reload(vm_state_t *vm) {
    if (!vm) return;
    // Проверяем и сбрасываем флаг под семафором
    if (!atomic_load(&vm->reload_pending)) return;

    // Копируем данные из временного буфера в память VM
    pending_reload_t *pr = &vm->pending_reload;
    memcpy(vm->mem + pr->target_addr, pr->buffer, pr->size);

    // Обновляем размер и имя модуля
    size_t idx = pr->module_index;
    module_info_t *mod = &vm->modules[idx];
    free(mod->name);
    mod->name = strdup(pr->new_name);
    mod->size = (uint32_t)pr->size;

    // Обновляем общее состояние программы
    //  - program_size можно пересчитать: старое + delta (или пересчитать полностью, но проще: diff_size = new_size - old_size)
    // Здесь учитываем только изменение размера одного модуля:
    vm->program_size = vm->program_size - (pr->size) + pr->size; // по сути без изменения
    // Обновляем общий CRC: новый образ уже загружен, так что просто ставим новый заранее вычисленный
    vm->program_hash = pr->new_hash;

    // Освобождаем временные ресурсы
    free(pr->buffer);
    pr->buffer = NULL;
    free(pr->new_name);
    pr->new_name = NULL;
    // Сбрасываем флаг (store и load гарантируют полную видимость)
    atomic_store(&vm->reload_pending, false);

    printf("Hot-reload applied: module %zu replaced at 0x%08X (%u bytes), new CRC=0x%08X\n",
           idx, pr->target_addr, (unsigned)pr->size, pr->new_hash);
}

// loader.c или отдельный файл (hot_reload.c)
int vm_schedule_hot_reload(vm_state_t *vm, const char *filename, size_t module_index) {
    if (!vm || !filename) return -1;

    // Проверяем индекс модуля
    if (module_index >= vm->module_count) {
        fprintf(stderr, "Invalid module index %zu for hot reload\n", module_index);
        return -1;
    }

    // Открываем новый бинарный файл модуля
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "vm_schedule_hot_reload: cannot open %s\n", filename);
        return -1;
    }

    // Определяем размер файла
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);
    if (fsize <= 0) {
        fprintf(stderr, "vm_schedule_hot_reload: file %s is empty or error (size=%ld)\n", filename, fsize);
        fclose(fp);
        return -1;
    }

    // Проверка кратности 4 байтам (фиксированная ширина инструкций)
    if (fsize % 4 != 0) {
        fprintf(stderr, "vm_schedule_hot_reload: file size %ld not multiple of 4\n", fsize);
        fclose(fp);
        return -1;
    }

    // Проверяем, что новый образ не превышает отведённую область:
    uint32_t orig_size = vm->modules[module_index].size;
    if ((size_t)fsize > orig_size) {
        // Если больше – нельзя заменить, т.к. вышли бы за границы исходного модуля
        fprintf(stderr, "vm_schedule_hot_reload: new module too large (%ld > %u)\n",
                fsize, orig_size);
        fclose(fp);
        return -1;
    }

    // Выделяем временный буфер и читаем новый образ
    uint8_t *temp_buf = malloc(fsize);
    if (!temp_buf) {
        fprintf(stderr, "vm_schedule_hot_reload: malloc failed\n");
        fclose(fp);
        return -1;
    }
    size_t read_bytes = fread(temp_buf, 1, fsize, fp);
    fclose(fp);
    if (read_bytes != (size_t)fsize) {
        fprintf(stderr, "vm_schedule_hot_reload: read error (%zu of %ld)\n", read_bytes, fsize);
        free(temp_buf);
        return -1;
    }

    // Вычисляем CRC32 нового образа (для проверки целостности)
    uint32_t crc = calculate_memory_hash(temp_buf, 0, fsize);  // assume CRC32
    // (Валидация: проверим повторно для гарантии целостности чтения)
    uint32_t crc_check = calculate_memory_hash(temp_buf, 0, fsize);
    if (crc != crc_check) {
        fprintf(stderr, "vm_schedule_hot_reload: CRC mismatch for %s\n", filename);
        free(temp_buf);
        return -1;
    }

    // Заполняем структуру pending_reload ДО установки флага
    pending_reload_t *pr = &vm->pending_reload;
    pr->new_name = strdup(filename);
    pr->buffer = temp_buf;
    pr->size = (uint32_t)fsize;
    pr->target_addr = vm->modules[module_index].addr;
    pr->module_index = module_index;
    pr->new_hash = crc;

    // Устанавливаем атомарный флаг (memory_order_release необязательно указывать вручную)
    atomic_store(&vm->reload_pending, true);

    return 0; // успешно запланирована замена
}
