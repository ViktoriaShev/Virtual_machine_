#include "test_common.h"
#include "hashing.h"

static int test_crc32_known_vector(void) {
    const char *s = "123456789";
    uint32_t crc = crc32(s, strlen(s));
    ASSERT_EQ_U32(0xCBF43926u, crc);
    return 0;
}

static int test_fnv1a_known_vector(void) {
    const char *s = "hello";
    uint32_t h = fnv1a32(s, strlen(s));
    ASSERT_EQ_U32(0x4F9F2CABu, h);
    return 0;
}

static int test_incremental_crc_matches_full(void) {
    const char *a = "bytecode";
    const char *b = "-engine";

    uint32_t full = crc32("bytecode-engine", strlen("bytecode-engine"));

    uint32_t inc = crc32_begin();
    inc = crc32_update(inc, a, strlen(a));
    inc = crc32_update(inc, b, strlen(b));
    inc = crc32_finalize(inc);

    ASSERT_EQ_U32(full, inc);
    return 0;
}

static int test_hash_buffer_selection(void) {
    const uint8_t buf[] = {1, 2, 3, 4, 5};
    uint32_t c = hash_buffer(buf, sizeof(buf), HASH_CRC32);
    uint32_t f = hash_buffer(buf, sizeof(buf), HASH_SIMPLE_FNV1A);

    ASSERT_EQ_U32(crc32(buf, sizeof(buf)), c);
    ASSERT_EQ_U32(fnv1a32(buf, sizeof(buf)), f);
    ASSERT_NEQ_U32(c, f);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_crc32_known_vector());
    ASSERT_EQ_U32(0, test_fnv1a_known_vector());
    ASSERT_EQ_U32(0, test_incremental_crc_matches_full());
    ASSERT_EQ_U32(0, test_hash_buffer_selection());

    printf("[PASS] hashing_unit_test\n");
    return 0;
}
