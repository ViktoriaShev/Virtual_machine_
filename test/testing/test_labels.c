#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/vm_tables.h"

/* We'll use vm_tables_init / labels_add / labels_lookup / vm_tables_destroy */

static int tests = 0;
static int fails = 0;
#define REPORT(name, cond) do { tests++; if (cond) printf("PASS: %s\n", name); else { printf("FAIL: %s (line %d)\n", name, __LINE__); fails++; } } while(0)

static void test_labels_basic(void) {
    vm_tables_init();

    labels_add("foo", 0x4000);
    uint32_t *p = labels_lookup("foo");
    REPORT("labels_lookup returns non-NULL for added label", p != NULL);
    if (p) {
        REPORT("labels_lookup returns correct address", *p == 0x4000u);
    }

    /* add another and lookup */
    labels_add("bar", 0x5000);
    uint32_t *q = labels_lookup("bar");
    REPORT("labels_lookup for second label", q != NULL && *q == 0x5000u);

    /* lookup non-existent label */
    uint32_t *nx = labels_lookup("no_such_label");
    REPORT("lookup of missing label returns NULL", nx == NULL);

    vm_tables_destroy();
}

int main(void) {
    printf("=== labels table tests ===\n");
    test_labels_basic();
    printf("==========================\n");
    if (fails == 0) {
        printf("ALL LABELS TESTS PASSED (%d)\n", tests);
        return 0;
    } else {
        printf("FAILED: %d / %d\n", fails, tests);
        return 1;
    }
}
