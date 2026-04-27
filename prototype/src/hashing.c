#define _POSIX_C_SOURCE 200809L  
#include "main.h"
#include "hashing.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ----------------------------
   Конфигурация 
   ---------------------------- */

#define MIN_TABLE_SIZE 8         // Минимальный размер
#define LOAD_FACTOR_MAX 0.5      // Увеличиваем при > 50%
#define LOAD_FACTOR_MIN 0.125    // Уменьшаем при < 12.5%

#define FNV1A32_OFFSET_BASIS 2166136261u
#define FNV1A32_PRIME        16777619u
/* ----------------------------
   Утилиты хеширования
   ---------------------------- */

/* ----------------------------
   CRC32 Implementation (IEEE 802.3)
   Глава 6: Heuristic Hash Functions
   ---------------------------- */

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

/* ----------------------------
   FNV-1a 32-bit
   ---------------------------- */

#define FNV1A32_OFFSET_BASIS 2166136261u
#define FNV1A32_PRIME        16777619u

uint32_t fnv1a32_begin(void) {
    return FNV1A32_OFFSET_BASIS;
}

uint32_t fnv1a32_update(uint32_t hash, const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;

    for (size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= FNV1A32_PRIME;
    }

    return hash;
}

uint32_t fnv1a32_finalize(uint32_t hash) {
    return hash;
}

uint32_t fnv1a32(const void *data, size_t length) {
    uint32_t h = fnv1a32_begin();
    h = fnv1a32_update(h, data, length);
    return fnv1a32_finalize(h);
}

uint32_t hash_buffer(const void *data, size_t length, hash_algorithm_t algo) {
    switch (algo) {
        case HASH_CRC32:
            return crc32(data, length);
        case HASH_SIMPLE_FNV1A:
            return fnv1a32(data, length);
        default:
            return crc32(data, length);
    }
}

uint32_t calculate_registers_hash_ex(
    const uint32_t *registers,
    size_t count,
    hash_algorithm_t algo
) {
    if (!registers) return 0;
    return hash_buffer(registers, count * sizeof(uint32_t), algo);
}

uint32_t calculate_memory_hash_ex(
    const uint8_t *memory,
    uint32_t start_addr,
    size_t length,
    hash_algorithm_t algo
) {
    if (!memory) return 0;
    return hash_buffer(memory + start_addr, length, algo);
}

uint32_t vm_calculate_registers_hash_ex(
    const vm_state_t *vm,
    hash_algorithm_t algo
) {
    if (!vm) return 0;
    return hash_buffer(vm->reg, REG_COUNT * sizeof(uint32_t), algo);
}

uint32_t vm_calculate_memory_hash_ex(
    const vm_state_t *vm,
    uint32_t start_addr,
    size_t length,
    hash_algorithm_t algo
) {
    if (!vm || !vm->mem) return 0;
    if (start_addr >= VM_MEM_BYTES) return 0;

    if (start_addr + length > VM_MEM_BYTES) {
        length = VM_MEM_BYTES - start_addr;
    }

    return hash_buffer(vm->mem + start_addr, length, algo);
}

uint32_t vm_calculate_program_hash_ex(
    const vm_state_t *vm,
    hash_algorithm_t algo
) {
    if (!vm || !vm->mem || vm->program_size == 0) return 0;
    if (vm->PC_START >= VM_MEM_BYTES) return 0;

    size_t hash_size = vm->program_size;
    if (hash_size > VM_MEM_BYTES - vm->PC_START) {
        hash_size = VM_MEM_BYTES - vm->PC_START;
    }

    return hash_buffer(vm->mem + vm->PC_START, hash_size, algo);
}

static void crc32_init_table(void) {
    const uint32_t polynomial = 0xEDB88320; // Reflected
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (uint32_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t crc32(const void *data, size_t length) {
    if (!crc32_initialized) {
        crc32_init_table();
    }
    
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t idx = (uint8_t)((crc ^ bytes[i]) & 0xFF);
        crc = (crc >> 8) ^ crc32_table[idx];
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t crc32_begin(void) {
    if (!crc32_initialized) crc32_init_table();
    return 0xFFFFFFFF;
}

uint32_t crc32_update(uint32_t crc, const void *data, size_t length) {
    if (!crc32_initialized) crc32_init_table();
    
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < length; i++) {
        uint8_t idx = (uint8_t)((crc ^ bytes[i]) & 0xFF);
        crc = (crc >> 8) ^ crc32_table[idx];
    }
    return crc;
}

uint32_t crc32_finalize(uint32_t crc) {
    return crc ^ 0xFFFFFFFF;
}

/* ----------------------------
   Probing Strategy (Linear Probing)
   Глава 3: лучшая кэш-эффективность
   ---------------------------- */

static inline uint32_t probe(uint32_t hash_key, uint32_t i, uint32_t m) {
    return (hash_key + i) & (m - 1);  // Битовая маска (m всегда степень 2)
}

/* ----------------------------
   Helper Functions (Глава 5)
   ---------------------------- */

static inline uint32_t hash_key_for(const hash_table_t *table, const void *key) {
    return table->key_type->hash(key);
}

static inline void *copy_key(const hash_table_t *table, const void *key) {
    return table->key_type->cpy(key);
}

static inline void *copy_val(const hash_table_t *table, const void *val) {
    return table->value_type->cpy(val);
}

static inline void free_key(const hash_table_t *table, void *key) {
    table->key_type->del(key);
}

static inline void free_val(const hash_table_t *table, void *val) {
    table->value_type->del(val);
}

static inline bool is_active_bin(const hash_bin_t *bin) {
    return bin->in_probe && !bin->is_empty;
}

/* Трёхуровневая проверка ключа */
static inline bool key_in_bin(
    const hash_table_t *table,
    const hash_bin_t *bin,
    uint32_t hash_key,
    const void *key
) {
    return is_active_bin(bin) &&                        // 1. Флаги (быстро)
           bin->hash_key == hash_key &&                 // 2. Хеш (быстро)
           table->key_type->cmp(bin->key, key);         // 3. Сравнение (медленно)
}

/* ----------------------------
   Search Functions
   ---------------------------- */

/* Найти bin с ключом или первый bin вне пробирования */
static hash_bin_t *find_key(
    hash_table_t *table,
    uint32_t hash_key,
    const void *key
) {
    for (uint32_t i = 0; i < table->size; i++) {
        hash_bin_t *bin = &table->bins[probe(hash_key, i, table->size)];
        
        if (!bin->in_probe || key_in_bin(table, bin, hash_key, key)) {
            return bin;
        }
    }
    
    assert(false); // Таблица не должна быть полной из-за resize
    return NULL;
}

/* Найти первый пустой bin в пробировании */
static hash_bin_t *find_empty(hash_table_t *table, uint32_t hash_key) {
    for (uint32_t i = 0; i < table->size; i++) {
        hash_bin_t *bin = &table->bins[probe(hash_key, i, table->size)];
        
        if (bin->is_empty) {
            return bin;
        }
    }
    
    assert(false);
    return NULL;
}

/* ----------------------------
   Memory Management
   ---------------------------- */

/* Освободить данные из bin */
static inline void free_bin(hash_table_t *table, hash_bin_t *bin) {
    if (is_active_bin(bin)) {
        free_key(table, bin->key);
        free_val(table, bin->val);
        
        bin->is_empty = true;  // in_probe остаётся true!
        table->active--;
    }
}

/* Сохранить данные в bin */
static inline void store_in_bin(
    hash_table_t *table,
    hash_bin_t *bin,
    uint32_t hash_key,
    void *key,
    void *val
) {
    /* Обновление счётчиков*/
    table->active += !!bin->is_empty;
    table->used += !bin->in_probe;
    
    /* Освободить старые данные */
    free_bin(table, bin);
    
    /* Записать новые */
    bin->in_probe = true;
    bin->is_empty = false;
    bin->hash_key = hash_key;
    bin->key = key;
    bin->val = val;
}

/* ----------------------------
   Forward declaration
   ---------------------------- */
static void hash_table_insert_internal(
    hash_table_t *table,
    uint32_t hash_key,
    void *key,
    void *val
);

/* ----------------------------
   Initialization and Resizing
   ---------------------------- */

/* Универсальная инициализация (для create и resize) */
static void init_table(
    hash_table_t *table,
    uint32_t size,
    hash_bin_t *old_bins_begin,
    hash_bin_t *old_bins_end
) {
    /* 1. Выделить новый массив */
    table->bins = (hash_bin_t *)calloc(size, sizeof(hash_bin_t));
    if (!table->bins) {
        fprintf(stderr, "Hash table allocation failed\n");
        exit(1);
    }
    
    table->size = size;
    table->used = 0;
    table->active = 0;
    
    /* 2. Инициализировать все bins */
    for (uint32_t i = 0; i < size; i++) {
        table->bins[i].in_probe = false;
        table->bins[i].is_empty = true;
    }
    
    /* 3. Перенести старые данные (если есть) */
    if (old_bins_begin) {
        for (hash_bin_t *bin = old_bins_begin; bin != old_bins_end; bin++) {
            if (!bin->is_empty) {
                /* Переиспользуем данные и хеш (не копируем!) */
                hash_table_insert_internal(table, bin->hash_key, 
                                          bin->key, bin->val);
            }
        }
    }
}

/* Изменить размер таблицы */
static void resize(hash_table_t *table, uint32_t new_size) {
    /* Сохранить ссылки на старые bins */
    hash_bin_t *old_bins_begin = table->bins;
    hash_bin_t *old_bins_end = old_bins_begin + table->size;
    
    /* Создать новую таблицу и перенести данные */
    init_table(table, new_size, old_bins_begin, old_bins_end);
    
    /* Освободить старый массив bins (но не данные - они перенесены!) */
    free(old_bins_begin);
}

/* ----------------------------
   Public API Implementation
   ---------------------------- */

hash_table_t *hash_table_create(
    const key_type_t *key_type,
    const value_type_t *value_type
) {
    hash_table_t *table = (hash_table_t *)malloc(sizeof(hash_table_t));
    if (!table) {
        return NULL;
    }
    
    table->key_type = key_type;
    table->value_type = value_type;
    
    init_table(table, MIN_TABLE_SIZE, NULL, NULL);
    
    return table;
}

void hash_table_destroy(hash_table_t *table) {
    if (!table) return;
    
    /* Освободить все данные в bins */
    for (uint32_t i = 0; i < table->size; i++) {
        free_bin(table, &table->bins[i]);
    }
    
    /* Освободить массив и таблицу */
    free(table->bins);
    free(table);
}

/* Внутренняя вставка (с уже скопированными данными) */
static void hash_table_insert_internal(
    hash_table_t *table,
    uint32_t hash_key,
    void *key,
    void *val
) {
    /* 1. Найти подходящий bin */
    hash_bin_t *bin = find_key(table, hash_key, key);
    
    if (!bin->in_probe) {
        /* Ключ не найден - ищем пустой bin */
        bin = find_empty(table, hash_key);
    }
    
    /* 2. Сохранить данные */
    store_in_bin(table, bin, hash_key, key, val);
    
    /* 3. Проверить load factor и resize при необходимости */
    if ((double)table->used / table->size > LOAD_FACTOR_MAX) {
        resize(table, table->size * 2);
    }
}

void hash_table_insert(
    hash_table_t *table,
    const void *key,
    const void *value
) {
    /* 1. Вычислить хеш */
    uint32_t hash_key = hash_key_for(table, key);
    
    /* 2. Скопировать данные (таблица берёт владение!) */
    void *key_copy = copy_key(table, key);
    void *val_copy = copy_val(table, value);
    
    /* 3. Вставить */
    hash_table_insert_internal(table, hash_key, key_copy, val_copy);
}

void *hash_table_lookup(hash_table_t *table, const void *key) {
    uint32_t hash_key = hash_key_for(table, key);
    hash_bin_t *bin = find_key(table, hash_key, key);
    
    /* Если bin в пробировании - ключ найден */
    return bin->in_probe ? bin->val : NULL;
}

void hash_table_delete(hash_table_t *table, const void *key) {
    uint32_t hash_key = hash_key_for(table, key);
    hash_bin_t *bin = find_key(table, hash_key, key);
    
    /* Освободить данные (если есть) */
    free_bin(table, bin);
    
    /* Проверить необходимость уменьшения */
    if (table->size > MIN_TABLE_SIZE &&
        (double)table->active / table->size < LOAD_FACTOR_MIN) {
        resize(table, table->size / 2);
    }
}

bool hash_table_contains(hash_table_t *table, const void *key) {
    return hash_table_lookup(table, key) != NULL;
}

/* ----------------------------
   Готовые типы для VM32
   ---------------------------- */

/* uint32_t ключи */
static uint32_t uint32_hash(const void *key) {
    uint32_t k = *(const uint32_t *)key;
    return crc32(&k, sizeof(k));
}

static bool uint32_compare(const void *a, const void *b) {
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

static void *uint32_copy(const void *key) {
    uint32_t *copy = (uint32_t *)malloc(sizeof(uint32_t));
    *copy = *(const uint32_t *)key;
    return copy;
}

static void uint32_free(void *key) {
    free(key);
}

const key_type_t uint32_key_type = {
    .hash = uint32_hash,
    .cmp = uint32_compare,
    .cpy = uint32_copy,
    .del = uint32_free
};

const value_type_t uint32_value_type = {
    .cpy = uint32_copy,
    .del = uint32_free
};

/* Строковые ключи */
static uint32_t string_hash(const void *key) {
    const char *str = (const char *)key;
    return crc32(str, strlen(str));
}

static bool string_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b) == 0;
}

static void *string_copy(const void *key) {
    return strdup((const char *)key);
}

static void string_free(void *key) {
    free(key);
}

const key_type_t string_key_type = {
    .hash = string_hash,
    .cmp = string_compare,
    .cpy = string_copy,
    .del = string_free
};

const value_type_t string_value_type = {
    .cpy = string_copy,
    .del = string_free
};

/* Указатели (без владения) */
static uint32_t ptr_hash(const void *key) {
    uintptr_t ptr = (uintptr_t)key;
    return crc32(&ptr, sizeof(ptr));
}

static bool ptr_compare(const void *a, const void *b) {
    return a == b;
}

static void *ptr_copy(const void *key) {
    return (void *)key;  // Не копируем!
}

static void ptr_free(void *key) {
    (void)key;  // Ничего не делаем
}

const key_type_t ptr_key_type = {
    .hash = ptr_hash,
    .cmp = ptr_compare,
    .cpy = ptr_copy,
    .del = ptr_free
};

const value_type_t ptr_value_type = {
    .cpy = ptr_copy,
    .del = ptr_free
};

/* ----------------------------
   VM32-специфичные функции хеширования
   ---------------------------- */

/* Общая версия (без VM instance) */
uint32_t calculate_registers_hash(const uint32_t *registers, size_t count) {
    return crc32(registers, count * sizeof(uint32_t));
}

/* Инстансная версия - хеш всех регистров VM */
uint32_t vm_calculate_registers_hash(const vm_state_t *vm) {
    if (!vm) return 0;
    return crc32(vm->reg, REG_COUNT * sizeof(uint32_t));
}

/* Общая версия (без VM instance) */
uint32_t calculate_memory_hash(
    const uint8_t *memory,
    uint32_t start_addr,
    size_t length
) {
    if (!memory) return 0;
    return crc32(memory + start_addr, length);
}

/* Инстансная версия - хеш блока памяти VM */
uint32_t vm_calculate_memory_hash(
    const vm_state_t *vm,
    uint32_t start_addr,
    size_t length
) {
    if (!vm || !vm->mem) return 0;
    if (start_addr >= VM_MEM_BYTES) return 0;
    
    /* Ограничить length границами памяти */
    if (start_addr + length > VM_MEM_BYTES) {
        length = VM_MEM_BYTES - start_addr;
    }
    
    return crc32(vm->mem + start_addr, length);
}

/* Проверка целостности данных */
bool verify_data_integrity(
    const void *data,
    size_t length,
    uint32_t expected_crc
) {
    return crc32(data, length) == expected_crc;
}

/* Хеш всего состояния VM (для snapshot/restore) */
uint32_t vm_calculate_state_hash(const vm_state_t *vm) {
    if (!vm) return 0;
    
    uint32_t crc = crc32_begin();
    
    /* 1. Регистры */
    crc = crc32_update(crc, vm->reg, REG_COUNT * sizeof(uint32_t));
    
    /* 2. PC */
    crc = crc32_update(crc, &vm->PC, sizeof(vm->PC));
    
    /* 3. Память (только используемая часть программы) */
    if (vm->program_size > 0 && vm->mem) {
        size_t mem_to_hash = vm->program_size;
        if (mem_to_hash > VM_MEM_BYTES) {
            mem_to_hash = VM_MEM_BYTES;
        }
        crc = crc32_update(crc, vm->mem, mem_to_hash);
    }
    
    /* 4. Таймеры (состояние) */
    crc = crc32_update(crc, vm->ton_timers, sizeof(vm->ton_timers));
    crc = crc32_update(crc, vm->tof_timers, sizeof(vm->tof_timers));
    crc = crc32_update(crc, vm->tp_timers, sizeof(vm->tp_timers));
    
    /* 5. Счётчики */
    crc = crc32_update(crc, vm->ctu_counters, sizeof(vm->ctu_counters));
    crc = crc32_update(crc, vm->ctd_counters, sizeof(vm->ctd_counters));
    crc = crc32_update(crc, vm->ctud_counters, sizeof(vm->ctud_counters));
    
    return crc32_finalize(crc);
}

/* Инкрементальное обновление хеша одного регистра */
void vm_update_register_hash(vm_state_t *vm, uint8_t reg_num) {
    if (!vm) return;
    
    /* Вычислить хеш конкретного регистра */
    vm->incremental.reg_hashes[reg_num] = crc32(&vm->reg[reg_num], sizeof(uint32_t));
}

/* Обновить хеши всех регистров */
void vm_update_all_register_hashes(vm_state_t *vm) {
    if (!vm) return;
    
    for (size_t i = 0; i < REG_COUNT; i++) {
        vm->incremental.reg_hashes[i] = crc32(&vm->reg[i], sizeof(uint32_t));
    }
    
    /* Также обновить общий инкрементальный хеш */
    vm->incremental.incremental_hash = vm_calculate_registers_hash(vm);
}

/* Вычислить хеш загруженной программы */
uint32_t vm_calculate_program_hash(const vm_state_t *vm) {
    if (!vm || !vm->mem || vm->program_size == 0) return 0;
    if (vm->PC_START >= VM_MEM_BYTES) return 0;

    size_t hash_size = vm->program_size;
    if (hash_size > VM_MEM_BYTES - vm->PC_START) {
        hash_size = VM_MEM_BYTES - vm->PC_START;
    }

    return crc32(vm->mem + vm->PC_START, hash_size);
}