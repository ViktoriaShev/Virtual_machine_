#ifndef HASHING_H
#define HASHING_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


/* ----------------------------
   Simple Hash Functions (FNV-1a)
   Быстрее CRC32, но менее надежен для обнаружения ошибок
   ---------------------------- */

/**
 * Вычисляет простой хеш (FNV-1a) для блока данных
 * Используйте для общих целей, когда скорость важнее надежности
 */
uint32_t simple_hash(const void* data, size_t length);

/* ----------------------------
   Unified Hash Interface
   Автоматически выбирает алгоритм на основе vm_config.hash_algo
   ---------------------------- */

/**
 * Вычисляет хеш состояния всех регистров (используя выбранный алгоритм)
 */
uint32_t calculate_registers_hash(void);

/**
 * Вычисляет хеш блока памяти (используя выбранный алгоритм)
 */
uint32_t calculate_memory_hash(uint32_t start_addr, size_t length);

/* ----------------------------
   CRC32 Functions (IEEE 802.3)
   ---------------------------- */

/**
 * Вычисляет CRC32 для блока данных
 * Наиболее надежный метод для контроля целостности
 * 
 * @param data    Указатель на данные
 * @param length  Размер данных в байтах
 * @return        32-битное значение CRC
 */
uint32_t crc32(const void* data, size_t length);

/**
 * Инкрементальное вычисление CRC32 (для больших данных)
 */
uint32_t crc32_begin(void);
uint32_t crc32_update(uint32_t crc, const void* data, size_t length);
uint32_t crc32_finalize(uint32_t crc);

/**
 * Проверка целостности данных по CRC32
 * 
 * @param data          Данные для проверки
 * @param length        Размер данных
 * @param expected_crc  Ожидаемое значение CRC
 * @return              true если CRC совпадает
 */
bool crc32_verify(const void* data, size_t length, uint32_t expected_crc);

/**
 * Специализированные функции для VM32
 */
uint32_t crc32_registers(const uint32_t* registers, size_t count);
uint32_t crc32_memory(const uint8_t* memory, uint32_t start_addr, size_t length);

/**
 * Проверяет целостность регистров
 */
bool verify_registers_integrity(uint32_t expected_hash);

/**
 * Проверяет целостность блока памяти
 */
bool verify_memory_integrity(uint32_t start_addr, size_t length, uint32_t expected_hash);

#endif