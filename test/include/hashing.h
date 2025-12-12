#ifndef HASHING_H
#define HASHING_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
   Hash Table Implementation based on "The Joys of Hashing" Chapter 5
   
   Features:
   - Open addressing with linear probing
   - Dynamic resizing (load factor 0.125 - 0.5)
   - CRC32 for hash keys (cached in bins)
   - Generic key/value support via function pointers
   - Proper memory ownership (table owns all data)
   ============================================================================ */

/* ----------------------------
   Типы функций для работы с ключами/значениями
   ---------------------------- */

/* Хеш-функция: преобразует ключ в hash_key */
typedef uint32_t (*hash_func_t)(const void *key);

/* Сравнение ключей: true если равны */
typedef bool (*compare_func_t)(const void *a, const void *b);

/* Копирование данных: возвращает копию */
typedef void *(*copy_func_t)(const void *data);

/* Освобождение памяти */
typedef void (*destructor_func_t)(void *data);

/* ----------------------------
   "Интерфейсы" типов (из Главы 5)
   ---------------------------- */

typedef struct {
    hash_func_t hash;        // Как хешировать
    compare_func_t cmp;      // Как сравнивать
    copy_func_t cpy;         // Как копировать
    destructor_func_t del;   // Как освобождать
} key_type_t;

typedef struct {
    copy_func_t cpy;         // Как копировать
    destructor_func_t del;   // Как освобождать
} value_type_t;

/* ----------------------------
   Структура bin (ячейка таблицы)
   Точно по книге, Глава 5
   ---------------------------- */

typedef struct {
    /* Флаги состояния (битовые поля) */
    int in_probe : 1;        // Ячейка в цепочке пробирования
    int is_empty : 1;        // Ячейка пуста (может быть in_probe после удаления)
    
    /* Кэшированный хеш-ключ (оптимизация из книги!) */
    uint32_t hash_key;
    
    /* Пользовательские данные */
    void *key;               // Ключ
    void *val;               // Значение
} hash_bin_t;

/* ----------------------------
   Структура хеш-таблицы
   ---------------------------- */

typedef struct {
    hash_bin_t *bins;        // Массив ячеек
    uint32_t size;           // Размер таблицы (всегда степень 2)
    uint32_t used;           // Кол-во bins в пробировании
    uint32_t active;         // Кол-во bins с данными
    
    /* Функции для работы с типами */
    const key_type_t *key_type;
    const value_type_t *value_type;
} hash_table_t;

/* ----------------------------
   Основные операции (API из Главы 5)
   ---------------------------- */

/* Создать таблицу */
hash_table_t *hash_table_create(
    const key_type_t *key_type,
    const value_type_t *value_type
);

/* Удалить таблицу (освободить всю память) */
void hash_table_destroy(hash_table_t *table);

/* Вставить или обновить ключ -> значение */
void hash_table_insert(
    hash_table_t *table,
    const void *key,
    const void *value
);

/* Найти значение по ключу (возвращает указатель или NULL) */
void *hash_table_lookup(
    hash_table_t *table,
    const void *key
);

/* Удалить ключ */
void hash_table_delete(
    hash_table_t *table,
    const void *key
);

/* Проверить наличие ключа */
bool hash_table_contains(
    hash_table_t *table,
    const void *key
);

/* Получить количество элементов */
static inline uint32_t hash_table_size(const hash_table_t *table) {
    return table->active;
}

/* Получить load factor */
static inline double hash_table_load(const hash_table_t *table) {
    return (double)table->used / (double)table->size;
}

/* ----------------------------
   CRC32 функции (промышленный стандарт)
   ---------------------------- */

uint32_t crc32(const void *data, size_t length);
uint32_t crc32_begin(void);
uint32_t crc32_update(uint32_t crc, const void *data, size_t length);
uint32_t crc32_finalize(uint32_t crc);

/* ----------------------------
   Готовые типы для VM32
   ---------------------------- */

/* Для uint32_t ключей */
extern const key_type_t uint32_key_type;
extern const value_type_t uint32_value_type;

/* Для строковых ключей */
extern const key_type_t string_key_type;
extern const value_type_t string_value_type;

/* Для указателей (без владения) */
extern const key_type_t ptr_key_type;
extern const value_type_t ptr_value_type;

/* ----------------------------
   Специализированные функции для VM32
   ---------------------------- */

/* Хеш состояния регистров */
uint32_t calculate_registers_hash(const uint32_t *registers, size_t count);

/* Хеш блока памяти */
uint32_t calculate_memory_hash(const uint8_t *memory, 
                                uint32_t start_addr, 
                                size_t length);

/* Проверка целостности */
bool verify_data_integrity(const void *data, 
                           size_t length, 
                           uint32_t expected_crc);

#endif /* HASHING_H */