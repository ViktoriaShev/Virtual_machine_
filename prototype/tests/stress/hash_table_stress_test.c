#include "test_common.h"
#include "hashing.h"

int main(void) {
    const uint32_t n = 10000;
    hash_table_t *t = hash_table_create(&uint32_key_type, &uint32_value_type);
    ASSERT_NOT_NULL(t);

    for (uint32_t i = 0; i < n; ++i) {
        uint32_t v = i ^ 0xA5A5A5A5u;
        hash_table_insert(t, &i, &v);
    }

    for (uint32_t i = 0; i < n; i += 7) {
        uint32_t *out = (uint32_t *)hash_table_lookup(t, &i);
        ASSERT_NOT_NULL(out);
        ASSERT_EQ_U32(i ^ 0xA5A5A5A5u, *out);
    }

    for (uint32_t i = 0; i < n; i += 2) {
        hash_table_delete(t, &i);
    }

    for (uint32_t i = 1; i < n; i += 2) {
        uint32_t *out = (uint32_t *)hash_table_lookup(t, &i);
        ASSERT_NOT_NULL(out);
        ASSERT_EQ_U32(i ^ 0xA5A5A5A5u, *out);
    }

    hash_table_destroy(t);
    printf("[PASS] hash_table_stress_test\n");
    return 0;
}
