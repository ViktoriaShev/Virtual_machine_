// loader.c
#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "hashing.h"   /* for calculate_memory_hash() */
#include "debug.h"     /* for opcode_name() if present (keeps debug output) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
  Задача: загрузить набор бинарников в память VM,
  заполнить modules/module_count, посчитать program_size и program_hash,
  и вывести карту модулей + первые инструкции для debug.
*/

/* Функция остаётся void — как в вашем проекте; при фатальной ошибке вызывает exit(). */
void load_programs(const char **fnames, int count) {
    if (count <= 0) {
        fprintf(stderr, "No program files specified\n");
        exit(1);
    }

    /* Сбрасываем счётчик модулей на начало (на случай повторного вызова) */
    module_count = 0;

    modules = (module_info_t *)calloc((size_t)count, sizeof(module_info_t));
    if (!modules) {
        fprintf(stderr, "Failed to allocate modules table\n");
        exit(1);
    }

    uint32_t write_ptr = PC_START;
    program_size = 0;
    program_hash = 0;

    for (int i = 0; i < count; ++i) {
        const char *fname = fnames[i];
        FILE *fp = fopen(fname, "rb");
        if (!fp) {
            fprintf(stderr, "Failed to open program file %s\n", fname);
            /* при ошибке освобождаем уже выделенные имена/модули */
            for (size_t j = 0; j < module_count; ++j) free(modules[j].name);
            free(modules);
            exit(1);
        }

        /* определяем размер файла */
        if (fseek(fp, 0, SEEK_END) != 0) {
            fprintf(stderr, "fseek failed for %s\n", fname);
            fclose(fp);
            for (size_t j = 0; j < module_count; ++j) free(modules[j].name);
            free(modules);
            exit(1);
        }
        long fsize = ftell(fp);
        if (fsize < 0) {
            fprintf(stderr, "ftell failed for %s\n", fname);
            fclose(fp);
            for (size_t j = 0; j < module_count; ++j) free(modules[j].name);
            free(modules);
            exit(1);
        }
        rewind(fp);

        if (fsize == 0) {
            /* пустой файл — регистрируем модуль, но не меняем память */
            fprintf(stderr, "Warning: file %s is empty (size=0)\n", fname);
            modules[module_count].name = strdup(fname);
            modules[module_count].addr = write_ptr;
            modules[module_count].size = 0;
            module_count++;
            fclose(fp);
            continue;
        }

        /* Проверяем, не выходим ли за память VM */
        if ((uint64_t)write_ptr + (uint64_t)fsize > (uint64_t)MEM_BYTES) {
            fprintf(stderr, "Error: loading %s would overflow VM memory (need %ld bytes, available %llu)\n",
                    fname, fsize, (unsigned long long)(MEM_BYTES - write_ptr));
            fclose(fp);
            for (size_t j = 0; j < module_count; ++j) free(modules[j].name);
            free(modules);
            exit(1);
        }

        size_t bytes_read = fread(mem + write_ptr, 1, (size_t)fsize, fp);
        if (bytes_read != (size_t)fsize) {
            fprintf(stderr, "Error reading file %s: read %zu of %ld\n", fname, bytes_read, fsize);
            fclose(fp);
            for (size_t j = 0; j < module_count; ++j) free(modules[j].name);
            free(modules);
            exit(1);
        }

        modules[module_count].name = strdup(fname);
        modules[module_count].addr = write_ptr;
        modules[module_count].size = (uint32_t)bytes_read;
        module_count++;

        printf("Loaded %zu bytes from %s into memory at 0x%08X\n", bytes_read, fname, write_ptr);

        write_ptr += (uint32_t)bytes_read;
        program_size += (uint32_t)bytes_read;

        fclose(fp);
    }

    if (module_count == 0) {
        fprintf(stderr, "ERROR: No modules were loaded successfully\n");
        free(modules);
        exit(1);
    }

    /* Вычисляем хеш всей программы (начиная с PC_START на program_size байт) */
    program_hash = calculate_memory_hash(mem, PC_START, program_size);

    /* печать карты модулей (user-friendly) */
    printf("\n=== Module Map ===\n");
    for (size_t i = 0; i < module_count; ++i) {
        printf("Module %zu: %s\n", i, modules[i].name);
        printf("  Address: 0x%08X\n", modules[i].addr);
        printf("  Size:    %u bytes\n", modules[i].size);
    }
    printf("==================\n\n");

    printf("Combined program size: %zu bytes\n", (size_t)program_size);
    printf("Combined program hash (CRC32): 0x%08X\n", program_hash);

    /* Для каждого модуля — распечатать первые 3 инструкции (если есть) для debug */
    for (size_t mi = 0; mi < module_count; ++mi) {
        uint32_t base = modules[mi].addr;
        uint32_t sz = modules[mi].size;
        printf("Module %zu: %s at 0x%08X (%u bytes)\n", mi, modules[mi].name, base, (unsigned)sz);
        for (size_t i = 0; i < 3 && (i * 4) < sz; ++i) {
            uint32_t addr = base + (uint32_t)(i * 4);
            uint32_t instr = mr32(addr);
            /* Если есть функция opcode_name — используем её; иначе печатаем просто 0xHEX */
#ifdef HAVE_OPCODE_NAME
            const char *opname = opcode_name(OPC(instr));
            printf("  [%s + 0x%04X] 0x%08X - %s\n",
                   modules[mi].name, (unsigned int)(i * 4), instr, opname ? opname : "(unknown)");
#else
            /* fallback: печатаем сырую инструкцию (hex) */
            printf("  [%s + 0x%04X] 0x%08X\n",
                   modules[mi].name, (unsigned int)(i * 4), instr);
#endif
        }
    }
}
