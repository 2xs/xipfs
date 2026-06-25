#ifndef TEST_H
#define TEST_H

#include <stddef.h>
#include <stdio.h>

typedef struct test_state_s {
    size_t total;
    size_t failed;
} test_state_t;

static inline void test_expect(test_state_t *ts, bool cond, const char *fmt, ...)
{
    ts->total++;
    if (!cond) {
        va_list ap;

        ts->failed++;
        fprintf(stderr, "[FAIL] ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "\n");
    }
}

static inline int test_report(const test_state_t *ts)
{
    printf("Test summary: %zu checks, %zu failures.\n", ts->total, ts->failed);
    if (ts->failed == 0UL) {
        printf("All tests passed.\n");
        return 0;
    }
    return 1;
}

#endif /* TEST_H */
