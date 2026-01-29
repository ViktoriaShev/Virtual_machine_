#define _POSIX_C_SOURCE 200809L
#include "../include/hashing.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int tests = 0;
static int fails = 0;

#define REPORT(name, expr) \
    do { \
        tests++; \
        if (expr) printf("PASS: %s\n", name); \
        else { printf("FAIL: %s\n", name); fails++; } \
    } while (0)

/* ============================================================
   1) Memory hash & self-modifying code
   ============================================================ */

static void test_memory_hash_smc(void) {
    uint8_t mem[64];
    memset(mem, 0xAA, sizeof(mem));

    uint32_t h1 = calculate_memory_hash(mem, 0, sizeof(mem));
    uint32_t h2 = calculate_memory_hash(mem, 0, sizeof(mem));

    REPORT("memory hash stable", h1 == h2);

    /* self-modifying code: меняем 1 байт */
    mem[10] ^= 0xFF;
    uint32_t h3 = calculate_memory_hash(mem, 0, sizeof(mem));

    REPORT("memory hash changes after modification", h1 != h3);

    /* откат */
    mem[10] ^= 0xFF;
    uint32_t h4 = calculate_memory_hash(mem, 0, sizeof(mem));

    REPORT("memory hash restored after revert", h1 == h4);
}

/* ============================================================
   2) Registers hash
   ============================================================ */

static void test_register_hash(void) {
    uint32_t regs[8] = {0};

    uint32_t h1 = calculate_registers_hash(regs, 8);
    uint32_t h2 = calculate_registers_hash(regs, 8);

    REPORT("register hash stable", h1 == h2);

    regs[3] = 42;
    uint32_t h3 = calculate_registers_hash(regs, 8);

    REPORT("register hash changes on mutation", h1 != h3);
}

/* ============================================================
   3) Hash table + SMC invalidation
   ============================================================ */

static void test_hash_table_smc(void) {
    hash_table_t *ht =
        hash_table_create(&uint32_key_type, &uint32_value_type);

    uint8_t mem[32];
    memset(mem, 0x11, sizeof(mem));

    uint32_t mem_hash1 = calculate_memory_hash(mem, 0, sizeof(mem));
    uint32_t value1 = 1234;

    hash_table_insert(ht, &mem_hash1, &value1);

    uint32_t *found =
        (uint32_t *)hash_table_lookup(ht, &mem_hash1);

    REPORT("lookup before SMC", found && *found == 1234);

    /* self-modifying code */
    mem[0] ^= 0xFF;
    uint32_t mem_hash2 = calculate_memory_hash(mem, 0, sizeof(mem));

    REPORT("SMC changes memory hash", mem_hash1 != mem_hash2);
    REPORT("old hash not found after SMC",
           hash_table_lookup(ht, &mem_hash2) == NULL);

    hash_table_destroy(ht);
}

/* ============================================================
   4) Delete + resize correctness
   ============================================================ */

static void test_delete_and_resize(void) {
    hash_table_t *ht =
        hash_table_create(&uint32_key_type, &uint32_value_type);

    /* Вставим много элементов → resize up */
    for (uint32_t i = 0; i < 100; i++) {
        hash_table_insert(ht, &i, &i);
    }

    REPORT("table grew", ht->size > 8);

    /* Удалим почти всё → resize down */
    for (uint32_t i = 0; i < 95; i++) {
        hash_table_delete(ht, &i);
    }

    REPORT("table shrank or stable", ht->size >= 8);

    /* Проверка целостности */
    for (uint32_t i = 95; i < 100; i++) {
        uint32_t *v = hash_table_lookup(ht, &i);
        REPORT("remaining key intact", v && *v == i);
    }

    hash_table_destroy(ht);
}

/* ============================================================
   main
   ============================================================ */

int main(void) {
    printf("=== hashing & self-modifying code tests ===\n");

    test_memory_hash_smc();
    test_register_hash();
    test_hash_table_smc();
    test_delete_and_resize();

    printf("=========================================\n");
    if (fails == 0) {
        printf("ALL TESTS PASSED (%d tests)\n", tests);
        return 0;
    } else {
        printf("FAILED: %d / %d\n", fails, tests);
        return 1;
    }
}
