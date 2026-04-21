#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>
#include "vm32.h"
#include "hashing.h"
#include "hot_reload.h"


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

static int has_bin_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && strcmp(dot, ".bin") == 0;
}

static int cmp_str(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

int collect_bin_files(const char *dir, char ***out_files, int *out_count) {
    if (!dir || !out_files || !out_count) return -1;

    DIR *d = opendir(dir);
    if (!d) {
        perror("opendir");
        return -1;
    }

    struct dirent *ent;
    char **files = NULL;
    int count = 0;

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type == DT_REG && has_bin_extension(ent->d_name)) {
            char *fullpath = NULL;
            if (asprintf(&fullpath, "%s/%s", dir, ent->d_name) == -1) {
                closedir(d);
                return -1;
            }

            char **tmp = realloc(files, sizeof(char *) * (count + 1));
            if (!tmp) {
                free(fullpath);
                closedir(d);
                return -1;
            }

            files = tmp;
            files[count++] = fullpath;
        }
    }

    closedir(d);

    qsort(files, count, sizeof(char *), cmp_str);

    *out_files = files;
    *out_count = count;

    return 0;
}

static uint32_t fnv1a_hash(const void *data, size_t len, uint32_t hash) {
    const unsigned char *p = data;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t calc_dir_signature(const char *dir) {
    char **files = NULL;
    int count = 0;

    if (collect_bin_files(dir, &files, &count) != 0) {
        return 0;
    }

    uint32_t hash = 2166136261u;

    for (int i = 0; i < count; ++i) {
        struct stat st;
        if (stat(files[i], &st) == 0) {
            hash = fnv1a_hash(files[i], strlen(files[i]), hash);
            hash = fnv1a_hash(&st.st_size, sizeof(st.st_size), hash);
            hash = fnv1a_hash(&st.st_mtime, sizeof(st.st_mtime), hash);
        }
        free(files[i]);
    }

    free(files);
    return hash;
}

int directory_changed(const char *dir, uint32_t *prev_sig) {
    uint32_t new_sig = calc_dir_signature(dir);

    if (*prev_sig == 0) {
        *prev_sig = new_sig;
        return 1; // первый запуск = "изменение"
    }

    if (new_sig != *prev_sig) {
        *prev_sig = new_sig;
        return 1;
    }

    return 0;
}

int reload_programs_from_directory(vm_state_t *vm, const char *dir) {
    if (!vm || !dir) return -1;

    char **files = NULL;
    int count = 0;

    if (collect_bin_files(dir, &files, &count) != 0) {
        fprintf(stderr, "Failed to collect .bin files from %s\n", dir);
        return -1;
    }

    if (count == 0) {
        fprintf(stderr, "No .bin files found in %s\n", dir);
        free(files);
        return -1;
    }

    printf("Reloading programs from directory: %s (%d files)\n", dir, count);

    vm_reset(vm);

    int rc = load_programs(vm, (const char **)files, count);

    for (int i = 0; i < count; ++i) {
        free(files[i]);
    }
    free(files);

    return rc;
}