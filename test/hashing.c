#include "vm32.h"
#include "hashing.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* ----------------------------
   Утилиты хеширования
   ---------------------------- */

/* Вычисляет хеш блока данных (FNV-1a алгоритм) */
uint32_t simple_hash(const void* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 0x811C9DC5;  // Начальное значение FNV-1a
    
    for (size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 0x01000193;
    }
    
    return hash;
}
/* ============================================================================
   CRC32 (IEEE 802.3 / Ethernet polynomial) Implementation
   
   Polynomial: 0x04C11DB7
   Initial value: 0xFFFFFFFF
   Final XOR: 0xFFFFFFFF
   Reflect input/output: Yes
   
   Используется в: Ethernet, ZIP, PNG, MPEG-2, и многих промышленных протоколах
   ============================================================================ */

/* ----------------------------
   CRC32 таблица (предвычисленная)
   ---------------------------- */
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

/* Инициализация таблицы CRC32 (вызывается автоматически при первом использовании) */
static void crc32_init_table(void) {
    const uint32_t polynomial = 0xEDB88320; // Reflected polynomial
    
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
    
    crc32_table_initialized = true;
}

/* ----------------------------
   Основная функция вычисления CRC32
   ---------------------------- */

/**
 * Вычисляет CRC32 для блока данных
 * 
 * @param data    Указатель на данные
 * @param length  Размер данных в байтах
 * @return        32-битное значение CRC
 */
uint32_t crc32(const void* data, size_t length) {
    if (!crc32_table_initialized) {
        crc32_init_table();
    }
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t table_index = (uint8_t)((crc ^ bytes[i]) & 0xFF);
        crc = (crc >> 8) ^ crc32_table[table_index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

/* ----------------------------
   Инкрементальное вычисление CRC32
   (для обработки данных частями)
   ---------------------------- */

/**
 * Начинает вычисление CRC32
 * @return Начальное значение CRC
 */
uint32_t crc32_begin(void) {
    if (!crc32_table_initialized) {
        crc32_init_table();
    }
    return 0xFFFFFFFF;
}

/**
 * Обновляет CRC32 новыми данными
 * 
 * @param crc     Текущее значение CRC
 * @param data    Указатель на новые данные
 * @param length  Размер новых данных
 * @return        Обновленное значение CRC
 */
uint32_t crc32_update(uint32_t crc, const void* data, size_t length) {
    if (!crc32_table_initialized) {
        crc32_init_table();
    }
    
    const uint8_t* bytes = (const uint8_t*)data;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t table_index = (uint8_t)((crc ^ bytes[i]) & 0xFF);
        crc = (crc >> 8) ^ crc32_table[table_index];
    }
    
    return crc;
}

/**
 * Завершает вычисление CRC32
 * @param crc Текущее значение CRC
 * @return Финальное значение CRC
 */
uint32_t crc32_finalize(uint32_t crc) {
    return crc ^ 0xFFFFFFFF;
}

/* ----------------------------
   Специализированные функции для VM32
   ---------------------------- */

/**
 * Вычисляет CRC32 всех регистров VM
 */
uint32_t crc32_registers(const uint32_t* registers, size_t count) {
    return crc32(registers, count * sizeof(uint32_t));
}


/* Вычисляет хеш состояния всех регистров (choose algorithm) */
uint32_t calculate_registers_hash(void) {
    /* Защита: если количество регов нулевое — вернуть 0 */
#ifdef REG_COUNT
    if (REG_COUNT == 0) return 0;
#endif

    if (vm_config.hash_algo == HASH_SIMPLE_FNV1A) {
        return simple_hash((const void*)reg, sizeof(reg));
    } else {
        /* По умолчанию — CRC32 */
        return crc32_registers(reg, REG_COUNT);
    }
}

/* Вычисляет хеш блока памяти (choose algorithm) */
uint32_t calculate_memory_hash(uint32_t start_addr, size_t length) {
    if (!mem) return 0;

    if (start_addr >= MEM_BYTES) return 0;

    if (start_addr + length > MEM_BYTES) {
        length = MEM_BYTES - start_addr;
    }

    if (vm_config.hash_algo == HASH_SIMPLE_FNV1A) {
        return simple_hash(mem + start_addr, length);
    } else {
        return crc32_memory(mem, start_addr, length);
    }
}

/* ----------------------------
   Специализированные функции для VM32 (реализации)
   ---------------------------- */

uint32_t crc32_memory(const uint8_t* memory, uint32_t start_addr, size_t length) {
    if (memory == NULL) return 0;
    if (start_addr >= MEM_BYTES) return 0;
    if (start_addr + length > MEM_BYTES) length = MEM_BYTES - start_addr;
    return crc32(memory + start_addr, length);
}


/**
 * Проверяет целостность данных по известному CRC32
 * 
 * @param data          Данные для проверки
 * @param length        Размер данных
 * @param expected_crc  Ожидаемое значение CRC
 * @return              true если CRC совпадает, false если повреждено
 */
bool crc32_verify(const void* data, size_t length, uint32_t expected_crc) {
    uint32_t calculated = crc32(data, length);
    return calculated == expected_crc;
}

bool verify_registers_integrity(uint32_t expected_hash) {
    uint32_t got = calculate_registers_hash();
    return got == expected_hash;
}

bool verify_memory_integrity(uint32_t start_addr, size_t length, uint32_t expected_hash) {
    uint32_t got = calculate_memory_hash(start_addr, length);
    return got == expected_hash;
}