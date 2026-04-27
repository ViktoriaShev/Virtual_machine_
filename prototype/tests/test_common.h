#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ_U32(expected, actual) do { \
    uint32_t _exp = (uint32_t)(expected); \
    uint32_t _act = (uint32_t)(actual); \
    if (_exp != _act) { \
        fprintf(stderr, "ASSERT_EQ_U32 failed at %s:%d: expected=%u actual=%u\n", \
                __FILE__, __LINE__, _exp, _act); \
        return 1; \
    } \
} while (0)

#define ASSERT_NEQ_U32(a, b) do { \
    uint32_t _a = (uint32_t)(a); \
    uint32_t _b = (uint32_t)(b); \
    if (_a == _b) { \
        fprintf(stderr, "ASSERT_NEQ_U32 failed at %s:%d: both=%u\n", \
                __FILE__, __LINE__, _a); \
        return 1; \
    } \
} while (0)

#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)

#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == NULL)

#define ASSERT_MEM_EQ(expected, actual, size) do { \
    if (memcmp((expected), (actual), (size)) != 0) { \
        fprintf(stderr, "ASSERT_MEM_EQ failed at %s:%d (size=%zu)\n", \
                __FILE__, __LINE__, (size_t)(size)); \
        return 1; \
    } \
} while (0)

#endif
