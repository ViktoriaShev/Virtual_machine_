// vm_tables.c
#define _POSIX_C_SOURCE 200809L

#include "vm32.h"
#include "vm_tables.h"
#include "hashing.h"   /* предполагает, что тут определены hash_table_* и key/value types */
#include "funcs.h"

#include "debug.h"     /* для log_file, если нужно писать лог */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* локальные таблицы */
static hash_table_t *labels = NULL;
static hash_table_t *breakpoints = NULL;
static hash_table_t *decoded_cache = NULL;

/* Инициализация таблиц (labels, breakpoints, decoded cache) */
void vm_tables_init(void) {
    /* string_key_type / uint32_key_type / ptr_value_type определены в проекте
       (раньше вы их использовали прямо из vm32.c) */
    labels = hash_table_create(&string_key_type, &uint32_value_type);
    breakpoints = hash_table_create(&uint32_key_type, &uint32_value_type);
    decoded_cache = hash_table_create(&uint32_key_type, &ptr_value_type);
}

/* Очистка (освобождает и значения, если value_type.del установлен) */
void vm_tables_destroy(void) {
    if (labels) {
        hash_table_destroy(labels);
        labels = NULL;
    }
    if (breakpoints) {
        hash_table_destroy(breakpoints);
        breakpoints = NULL;
    }
    if (decoded_cache) {
        hash_table_destroy(decoded_cache);
        decoded_cache = NULL;
    }
}

/* labels API */
void labels_add(const char *name, uint32_t addr) {
    if (!labels) return;
    hash_table_insert(labels, name, &addr);
}

uint32_t *labels_lookup(const char *name) {
    if (!labels) return NULL;
    return (uint32_t *)hash_table_lookup(labels, name);
}

/* breakpoints API */
void bp_set(uint32_t addr) {
    if (!breakpoints) return;
    uint32_t v = 1;
    hash_table_insert(breakpoints, &addr, &v);
}

void bp_clear(uint32_t addr) {
    if (!breakpoints) return;
    hash_table_delete(breakpoints, &addr);
}

bool bp_is_set(uint32_t addr) {
    if (!breakpoints) return false;
    return hash_table_contains(breakpoints, &addr);
}

/* -------------------------
   Кеш декодированных инструкций
   -------------------------
   vm_decode_instruction — возвращает decoded_instr_t* для адреса (кеширует)
*/
static decoded_instr_t *decode_instruction_cached(uint32_t addr) {
    if (!decoded_cache) return NULL;

    uint32_t key = addr;
    decoded_instr_t *cached = (decoded_instr_t *)hash_table_lookup(decoded_cache, &key);
    if (cached) return cached;

    /* безопасность: проверяем, что можно читать mr32(addr) */
    if (addr >= MEM_BYTES - 3) return NULL;

    uint32_t instr = mr32(addr);

    decoded_instr_t *dec = (decoded_instr_t *)malloc(sizeof(decoded_instr_t));
    if (!dec) {
        fprintf(stderr, "vm_tables: malloc failed for decoded_instr\n");
        return NULL;
    }

    dec->raw_instr = instr;
    dec->opcode = OPC(instr);
    dec->ra = RA(instr);
    dec->rb = RB(instr);
    dec->rc = RC(instr);
    dec->has_immediate = FIMM(instr) ? true : false;
    dec->immediate = dec->has_immediate ? IMM8(instr) : 0;

    /* Вставляем в кеш (hash_table_insert скопирует ключ; value-type для ptr должен free() при destroy) */
    hash_table_insert(decoded_cache, &key, dec);

    return dec;
}

/* экспортированная обёртка (прототип в vm32.h) */
decoded_instr_t *vm_decode_instruction(uint32_t addr) {
    return decode_instruction_cached(addr);
}
