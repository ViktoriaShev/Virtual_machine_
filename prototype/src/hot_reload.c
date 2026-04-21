#define _POSIX_C_SOURCE 200809L

#include "hot_reload.h"
#include "vm32.h"
#include "loader.h"
#include "hashing.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int has_bin_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && strcmp(dot, ".bin") == 0;
}

static char *join_path(const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    size_t need = dir_len + 1 + name_len + 1;

    char *full = (char *)malloc(need);
    if (!full) return NULL;

    int written = snprintf(full, need, "%s/%s", dir, name);
    if (written < 0 || (size_t)written >= need) {
        free(full);
        return NULL;
    }

    return full;
}

static int cmp_cstr_ptr(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

static int push_path(char ***files, int *count, const char *path) {
    char **tmp = (char **)realloc(*files, sizeof(char *) * (size_t)(*count + 1));
    if (!tmp) return -1;

    *files = tmp;
    (*files)[*count] = strdup(path);
    if (!(*files)[*count]) return -1;

    (*count)++;
    return 0;
}

static int file_is_regular(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int collect_bin_files(const char *dir, char ***out_files, int *out_count) {
    if (!dir || !out_files || !out_count) return -1;

    *out_files = NULL;
    *out_count = 0;

    DIR *d = opendir(dir);
    if (!d) {
        perror("opendir");
        return -1;
    }

    struct dirent *ent;
    char **files = NULL;
    int count = 0;

    while ((ent = readdir(d)) != NULL) {
        if (!has_bin_extension(ent->d_name)) continue;
        if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' ||
            (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
            continue;
        }

        char *full = join_path(dir, ent->d_name);
        if (!full) {
            closedir(d);
            for (int i = 0; i < count; ++i) free(files[i]);
            free(files);
            return -1;
        }

        if (!file_is_regular(full)) {
            free(full);
            continue;
        }

        if (push_path(&files, &count, full) != 0) {
            free(full);
            closedir(d);
            for (int i = 0; i < count; ++i) free(files[i]);
            free(files);
            return -1;
        }

        free(full);
    }

    closedir(d);

    if (count > 1) {
        qsort(files, (size_t)count, sizeof(char *), cmp_cstr_ptr);
    }

    *out_files = files;
    *out_count = count;
    return 0;
}

static uint32_t calc_dir_signature(const char *dir) {
    char **files = NULL;
    int count = 0;

    if (collect_bin_files(dir, &files, &count) != 0) {
        return 0;
    }

    uint32_t crc = crc32_begin();

    for (int i = 0; i < count; ++i) {
        struct stat st;
        if (stat(files[i], &st) != 0) {
            free(files[i]);
            continue;
        }

        crc = crc32_update(crc, files[i], strlen(files[i]));
        crc = crc32_update(crc, &st.st_size, sizeof(st.st_size));
        crc = crc32_update(crc, &st.st_mtime, sizeof(st.st_mtime));

        FILE *fp = fopen(files[i], "rb");
        if (fp) {
            uint8_t buf[4096];
            for (;;) {
                size_t n = fread(buf, 1, sizeof(buf), fp);
                if (n > 0) {
                    crc = crc32_update(crc, buf, n);
                }
                if (n < sizeof(buf)) break;
            }
            fclose(fp);
        }

        free(files[i]);
    }

    free(files);
    return crc32_finalize(crc);
}

int directory_changed(const char *dir, uint32_t *prev_sig) {
    if (!dir || !prev_sig) return -1;

    uint32_t new_sig = calc_dir_signature(dir);

    if (*prev_sig == 0) {
        *prev_sig = new_sig;
        return 0;
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

    if (count <= 0) {
        fprintf(stderr, "No .bin files found in %s\n", dir);
        free(files);
        return -1;
    }

    const char **fnames = (const char **)malloc(sizeof(char *) * (size_t)count);
    if (!fnames) {
        for (int i = 0; i < count; ++i) free(files[i]);
        free(files);
        return -1;
    }

    for (int i = 0; i < count; ++i) {
        fnames[i] = files[i];
    }

    int rc = load_programs(vm, fnames, count);

    for (int i = 0; i < count; ++i) free(files[i]);
    free(files);
    free(fnames);

    if (rc == 0) {
        vm->program_hash = vm_calculate_program_hash(vm);
    }

    return rc;
}

void apply_pending_reload(vm_state_t *vm) {
    if (!vm || !vm->program_dir) return;

    if (!atomic_exchange(&vm->reload_pending, false)) {
        return;
    }

    if (reload_programs_from_directory(vm, vm->program_dir) != 0) {
        fprintf(stderr, "Hot-reload failed for directory %s\n", vm->program_dir);
        atomic_store(&vm->reload_pending, true);
        return;
    }

    printf("Hot-reload applied from directory: %s\n", vm->program_dir);
}