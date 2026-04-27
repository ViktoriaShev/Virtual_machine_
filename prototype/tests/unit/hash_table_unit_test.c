#include "test_common.h"
#include "hashing.h"

static int test_insert_lookup_update_delete(void) {
    hash_table_t *t = hash_table_create(&uint32_key_type, &uint32_value_type);
    ASSERT_NOT_NULL(t);

    uint32_t k = 42;
    uint32_t v1 = 10;
    uint32_t v2 = 99;

    hash_table_insert(t, &k, &v1);
    ASSERT_TRUE(hash_table_contains(t, &k));

    uint32_t *out = (uint32_t *)hash_table_lookup(t, &k);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ_U32(v1, *out);

    hash_table_insert(t, &k, &v2);
    out = (uint32_t *)hash_table_lookup(t, &k);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ_U32(v2, *out);

    hash_table_delete(t, &k);
    ASSERT_FALSE(hash_table_contains(t, &k));

    hash_table_destroy(t);
    return 0;
}

static int test_resize_and_data_survival(void) {
    hash_table_t *t = hash_table_create(&uint32_key_type, &uint32_value_type);
    ASSERT_NOT_NULL(t);

    for (uint32_t i = 0; i < 200; ++i) {
        uint32_t v = i * 3;
        hash_table_insert(t, &i, &v);
    }

    ASSERT_TRUE(hash_table_size(t) >= 200);

    for (uint32_t i = 0; i < 200; ++i) {
        uint32_t *out = (uint32_t *)hash_table_lookup(t, &i);
        ASSERT_NOT_NULL(out);
        ASSERT_EQ_U32(i * 3, *out);
    }

    for (uint32_t i = 0; i < 150; ++i) {
        hash_table_delete(t, &i);
    }

    for (uint32_t i = 150; i < 200; ++i) {
        uint32_t *out = (uint32_t *)hash_table_lookup(t, &i);
        ASSERT_NOT_NULL(out);
        ASSERT_EQ_U32(i * 3, *out);
    }

    hash_table_destroy(t);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_insert_lookup_update_delete());
    ASSERT_EQ_U32(0, test_resize_and_data_survival());

    printf("[PASS] hash_table_unit_test\n");
    return 0;
}
