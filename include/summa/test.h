#ifndef SUMMA_TEST_H
#define SUMMA_TEST_H

/* summa/test.h — unit testing framework. See docs/test/DOCS.md. */

#include <stdio.h>
#include <string.h>

typedef struct {
    const char* suite;
    int         tests_passed;
    int         tests_failed;
    int         _assert_failed;
} summa_test_ctx_t;

typedef struct {
    const char* file;
    int         line;
    const char* expr;
    const char* message;
} summa_test_failure_t;

extern summa_test_ctx_t summa_test_ctx;

void summa_test_begin(const char* suite_name);
int  summa_test_end(void);
void summa_test_assert_fail(const summa_test_failure_t* f);

#define SUMMA_TEST_ASSERT(expr)                                          \
    do {                                                                 \
        if (!(expr)) {                                                   \
            summa_test_failure_t _f = {__FILE__, __LINE__, #expr, NULL}; \
            summa_test_assert_fail(&_f);                                 \
        }                                                                \
    } while (0)

#define SUMMA_TEST_ASSERT_MSG(expr, msg)                                  \
    do {                                                                  \
        if (!(expr)) {                                                    \
            summa_test_failure_t _f = {__FILE__, __LINE__, #expr, (msg)}; \
            summa_test_assert_fail(&_f);                                  \
        }                                                                 \
    } while (0)

#define SUMMA_TEST_ASSERT_EQ(a, b) SUMMA_TEST_ASSERT((a) == (b))
#define SUMMA_TEST_ASSERT_NEQ(a, b) SUMMA_TEST_ASSERT((a) != (b))

#define SUMMA_TEST_ASSERT_EQ_INT(expected, actual)                                          \
    do {                                                                                    \
        int _e = (expected), _a = (actual);                                                 \
        if (_e != _a) {                                                                     \
            char _buf[128];                                                                 \
            snprintf(_buf, sizeof(_buf), "expected %d, got %d", _e, _a);                    \
            summa_test_failure_t _f = {__FILE__, __LINE__, #expected " == " #actual, _buf}; \
            summa_test_assert_fail(&_f);                                                    \
        }                                                                                   \
    } while (0)

#define SUMMA_TEST_ASSERT_EQ_STR(expected, actual)                                                               \
    do {                                                                                                         \
        const char *_e = (expected), *_a = (actual);                                                             \
        if (!_e || !_a || strcmp(_e, _a) != 0) {                                                                 \
            char _buf[256];                                                                                      \
            snprintf(_buf, sizeof(_buf), "expected \"%s\", got \"%s\"", _e ? _e : "(null)", _a ? _a : "(null)"); \
            summa_test_failure_t _f = {__FILE__, __LINE__, #expected " == " #actual, _buf};                      \
            summa_test_assert_fail(&_f);                                                                         \
        }                                                                                                        \
    } while (0)

#define SUMMA_TEST_ASSERT_NULL(ptr) SUMMA_TEST_ASSERT((ptr) == nullptr)
#define SUMMA_TEST_ASSERT_NOT_NULL(ptr) SUMMA_TEST_ASSERT((ptr) != nullptr)

#define SUMMA_TEST_RUN(fn)                   \
    do {                                     \
        summa_test_ctx._assert_failed = 0;   \
        (fn)();                              \
        if (summa_test_ctx._assert_failed) { \
            summa_test_ctx.tests_failed++;   \
            printf("  FAIL  %s\n", #fn);     \
        } else {                             \
            summa_test_ctx.tests_passed++;   \
            printf("  ok    %s\n", #fn);     \
        }                                    \
    } while (0)

#endif /* SUMMA_TEST_H */

#ifdef SUMMA_TEST_IMPLEMENTATION

summa_test_ctx_t summa_test_ctx;

void summa_test_begin(const char* suite_name) {
    summa_test_ctx = (summa_test_ctx_t){
        .suite          = suite_name,
        .tests_passed   = 0,
        .tests_failed   = 0,
        ._assert_failed = 0,
    };
    printf("=== %s ===\n", suite_name);
}

int summa_test_end(void) {
    int total = summa_test_ctx.tests_passed + summa_test_ctx.tests_failed;
    printf("--- %d / %d passed", summa_test_ctx.tests_passed, total);
    if (summa_test_ctx.tests_failed > 0)
        printf("  (%d FAILED)", summa_test_ctx.tests_failed);
    printf(" ---\n");
    return summa_test_ctx.tests_failed > 0 ? 1 : 0;
}

void summa_test_assert_fail(const summa_test_failure_t* f) {
    summa_test_ctx._assert_failed = 1;
    printf("    ASSERT  %s  (%s:%d)", f->expr, f->file, f->line);
    if (f->message)
        printf("\n    ->  %s", f->message);
    printf("\n");
}

#endif /* SUMMA_TEST_IMPLEMENTATION */
