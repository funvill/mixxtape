#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <string.h>

extern int g_failures;
extern int g_checks;

#define CHECK(cond)                                                            \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        g_checks++;                                                            \
        long long _a = (long long)(a), _b = (long long)(b);                    \
        if (_a != _b) {                                                        \
            printf("  FAIL %s:%d  %s == %s  (%lld vs %lld)\n", __FILE__,       \
                   __LINE__, #a, #b, _a, _b);                                  \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_MEM(a, b, n)                                                     \
    do {                                                                       \
        g_checks++;                                                            \
        if (memcmp((a), (b), (n)) != 0) {                                      \
            printf("  FAIL %s:%d  memcmp(%s, %s, %s)\n", __FILE__, __LINE__,   \
                   #a, #b, #n);                                                \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

#define RUN(fn)                                                                \
    do {                                                                       \
        int before = g_failures;                                               \
        printf("== %s\n", #fn);                                                \
        fn();                                                                  \
        if (g_failures == before) {                                            \
            printf("   ok\n");                                                 \
        }                                                                      \
    } while (0)

#define TEST_MAIN_BEGIN()                                                      \
    int g_failures = 0;                                                        \
    int g_checks = 0;                                                          \
    int main(void)                                                             \
    {
#define TEST_MAIN_END()                                                        \
    printf("\n%d checks, %d failures\n", g_checks, g_failures);                \
    return g_failures ? 1 : 0;                                                 \
    }

#endif /* TEST_UTIL_H */
