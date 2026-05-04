// vm_tables.c
#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "vm_tables.h"
#include "hashing.h"   /* предполагает, что тут определены hash_table_* и key/value types */
#include "funcs.h"

#include "debug.h"     /* для log_file, если нужно писать лог */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


int vm_tables_init(vm_state_t *vm) {
    if (!vm) return -1;

    /* создаём таблицы и сохраняем в vm */
    vm->labels = hash_table_create(&string_key_type, &uint32_value_type);
    if (!vm->labels) return -1;

    vm->breakpoints = hash_table_create(&uint32_key_type, &uint32_value_type);
    if (!vm->breakpoints) {
        hash_table_destroy(vm->labels);
        vm->labels = NULL;
        return -1;
    }

    vm->decoded_cache = hash_table_create(&uint32_key_type, &ptr_value_type);
    if (!vm->decoded_cache) {
        hash_table_destroy(vm->labels);
        hash_table_destroy(vm->breakpoints);
        vm->labels = vm->breakpoints = NULL;
        return -1;
    }

    return 0;
}

void vm_tables_destroy(vm_state_t *vm) {
    if (!vm) return;

    if (vm->labels) {
        hash_table_destroy(vm->labels);
        vm->labels = NULL;
    }
    if (vm->breakpoints) {
        hash_table_destroy(vm->breakpoints);
        vm->breakpoints = NULL;
    }
    if (vm->decoded_cache) {
        /* В value_type для ptr_value_type должен быть установлен делитер,
           который корректно free() значений (decoded_instr_t*). Если он
           не установлен в вашей реализации hash_table, нужно вручную
           итерировать и free() значения здесь — но мы предполагаем, что
           ptr_value_type обрабатывает это. */
        hash_table_destroy(vm->decoded_cache);
        vm->decoded_cache = NULL;
    }
}

/* labels API */
void labels_add(vm_state_t *vm, const char *name, uint32_t addr) {
    if (!vm || !vm->labels) return;
    /* hash_table_insert скопирует ключ/значение согласно типам ключа/значения */
    hash_table_insert(vm->labels, name, &addr);
}

uint32_t *labels_lookup(vm_state_t *vm, const char *name) {
    if (!vm || !vm->labels) return NULL;
    return (uint32_t *)hash_table_lookup(vm->labels, name);
}

/* breakpoints API */
void bp_set(vm_state_t *vm, uint32_t addr) {
    if (!vm || !vm->breakpoints) return;
    uint32_t v = 1;
    hash_table_insert(vm->breakpoints, &addr, &v);
}

void bp_clear(vm_state_t *vm, uint32_t addr) {
    if (!vm || !vm->breakpoints) return;
    hash_table_delete(vm->breakpoints, &addr);
}

bool bp_is_set(vm_state_t *vm, uint32_t addr) {
    if (!vm || !vm->breakpoints) return false;
    return hash_table_contains(vm->breakpoints, &addr);
}

/* -------------------------
   Кеш декодированных инструкций (инстансный)
   ------------------------- */
static decoded_instr_t *decode_instruction_cached(vm_state_t *vm, uint32_t addr) {
    if (!vm || !vm->decoded_cache) return NULL;

    uint32_t key = addr;
    decoded_instr_t *cached = (decoded_instr_t *)hash_table_lookup(vm->decoded_cache, &key);
    if (cached) return cached;

    /* безопасность: проверяем, что можно читать vm_mr32(addr) */
    if (addr >= VM_MEM_BYTES - 3) return NULL;

    uint32_t instr = vm_mr32(vm, addr);

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

    /* Вставляем в кеш (hash_table_insert скопирует ключ; для value ptr должно быть настроено освобождение) */
    hash_table_insert(vm->decoded_cache, &key, dec);

    return dec;
}

/* экспортированная обёртка */
decoded_instr_t *vm_decode_instruction(vm_state_t *vm, uint32_t addr) {
    return decode_instruction_cached(vm, addr);
}
