#ifndef SUMMA_TEST_H
#define SUMMA_TEST_H

/* test.h — unit testing framework. See docs/test/DOCS.md. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* suite;
    int         tests_passed;
    int         tests_failed;
    int         _assert_failed;
    const char* _filter;
    int         _list_mode;
} summa_test_ctx_t;

typedef struct {
    const char* file;
    int         line;
    const char* expr;
    const char* message;
} summa_test_failure_t;

extern summa_test_ctx_t summa_test_ctx;

/* argc/argv are the ones passed to main. With no extra args, every test in the
 * suite runs. `<binary> <test_name>` runs only that test. `<binary> --list`
 * prints every test name in the suite, one per line, and runs nothing. */
void summa_test_begin(const char* suite_name, int argc, char** argv);
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

#define SUMMA_TEST_TODO(msg) SUMMA_TEST_ASSERT_MSG(false, "TODO: " msg)

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

#define SUMMA_TEST_ASSERT_EQ_STR(expected, actual)                                                                  \
    do {                                                                                                            \
        const char *_e = (expected), *_a = (actual);                                                                \
        if (!_e || !_a || strcmp(_e, _a) != 0) {                                                                    \
            char _buf[256];                                                                                         \
            snprintf(                                                                                               \
                _buf, sizeof(_buf), "expected \"%.100s\", got \"%.100s\"", _e ? _e : "(null)", _a ? _a : "(null)"); \
            summa_test_failure_t _f = {__FILE__, __LINE__, #expected " == " #actual, _buf};                         \
            summa_test_assert_fail(&_f);                                                                            \
        }                                                                                                           \
    } while (0)

#define SUMMA_TEST_ASSERT_NULL(ptr) SUMMA_TEST_ASSERT((ptr) == nullptr)
#define SUMMA_TEST_ASSERT_NOT_NULL(ptr) SUMMA_TEST_ASSERT((ptr) != nullptr)

typedef struct {
    FILE* file;
} summa_test_file_t;

summa_test_file_t summa_test_file_open(void);
void              summa_test_file_close(summa_test_file_t* tf);
size_t            summa_test_file_read(summa_test_file_t* tf, char* buf, size_t buf_size);

/* Opens a scoped FILE* capture: `var.file` is writable inside the block and is
 * automatically closed when the block exits. */
#define SUMMA_TEST_SCOPED_FILE(var)                                                                                 \
    for (summa_test_file_t var = summa_test_file_open(), *_summa_scope_##var = &var; _summa_scope_##var != nullptr; \
         summa_test_file_close(&var), _summa_scope_##var                     = nullptr)

/* Asserts everything written to `tf` since it was opened (or last asserted on)
 * equals `expected`, then drains the capture. */
#define SUMMA_TEST_ASSERT_FILE_EQ_STR(tf, expected)      \
    do {                                                 \
        char _buf[256];                                  \
        summa_test_file_read(&(tf), _buf, sizeof(_buf)); \
        SUMMA_TEST_ASSERT_EQ_STR((expected), _buf);      \
    } while (0)

#define SUMMA_TEST_ASSERT_FILE_EQ_CHAR(tf, expected)     \
    do {                                                 \
        char _buf[2];                                    \
        summa_test_file_read(&(tf), _buf, sizeof(_buf)); \
        SUMMA_TEST_ASSERT_EQ((expected), _buf[0]);       \
    } while (0)

void    summa_test_random_seed();
double  summa_test_random_double_between(double min, double max);
int64_t summa_test_random_integer_between(int64_t min, int64_t max);

#define SUMMA_TEST_RUN(fn)                                                      \
    do {                                                                        \
        if (summa_test_ctx._list_mode) {                                        \
            printf("%s\n", #fn);                                                \
            break;                                                              \
        }                                                                       \
        if (summa_test_ctx._filter && strcmp(summa_test_ctx._filter, #fn) != 0) \
            break;                                                              \
        summa_test_ctx._assert_failed = 0;                                      \
        (fn)();                                                                 \
        if (summa_test_ctx._assert_failed) {                                    \
            summa_test_ctx.tests_failed++;                                      \
            printf("  FAIL  %s\n", #fn);                                        \
        } else {                                                                \
            summa_test_ctx.tests_passed++;                                      \
            printf("  ok    %s\n", #fn);                                        \
        }                                                                       \
    } while (0)

#endif /* SUMMA_TEST_H */

#ifdef SUMMA_TEST_IMPLEMENTATION
#ifndef SUMMA_TEST_IMPLEMENTATION_ONCE
#define SUMMA_TEST_IMPLEMENTATION_ONCE

#include <stdlib.h>
#include <time.h>

summa_test_ctx_t summa_test_ctx;

void summa_test_begin(const char* suite_name, int argc, char** argv) {
    summa_test_ctx = (summa_test_ctx_t){
        .suite          = suite_name,
        .tests_passed   = 0,
        .tests_failed   = 0,
        ._assert_failed = 0,
        ._filter        = nullptr,
        ._list_mode     = 0,
    };
    if (argc > 1) {
        if (strcmp(argv[1], "--list") == 0)
            summa_test_ctx._list_mode = 1;
        else
            summa_test_ctx._filter = argv[1];
    }
    if (!summa_test_ctx._list_mode)
        printf("=== %s ===\n", suite_name);
}

int summa_test_end(void) {
    if (summa_test_ctx._list_mode)
        return 0;
    int total = summa_test_ctx.tests_passed + summa_test_ctx.tests_failed;
    printf("--- %d / %d passed", summa_test_ctx.tests_passed, total);
    if (summa_test_ctx.tests_failed > 0)
        printf("  (%d FAILED)", summa_test_ctx.tests_failed);
    printf(" ---\n");
    return summa_test_ctx.tests_failed > 0 ? 1 : 0;
}

/* GCC-style "file:line: error: message" — recognized as-is by editors and
 * tools that link file:line references (including the default errorPattern
 * in VS Code's "CMake Test Explorer" extension). */
void summa_test_assert_fail(const summa_test_failure_t* f) {
    summa_test_ctx._assert_failed = 1;
    if (f->message)
        printf("%s:%d: error: %s (%s)\n", f->file, f->line, f->expr, f->message);
    else
        printf("%s:%d: error: %s\n", f->file, f->line, f->expr);
}

summa_test_file_t summa_test_file_open(void) {
    return (summa_test_file_t){.file = tmpfile()};
}

void summa_test_file_close(summa_test_file_t* tf) {
    if (tf->file) {
        fclose(tf->file);
        tf->file = nullptr;
    }
}

size_t summa_test_file_read(summa_test_file_t* tf, char* buf, size_t buf_size) {
    fflush(tf->file);
    long pos = ftell(tf->file);
    rewind(tf->file);

    size_t to_read = (pos > 0 && (size_t)pos < buf_size) ? (size_t)pos : buf_size - 1;
    size_t n       = fread(buf, 1, to_read, tf->file);
    buf[n]         = '\0';

    /* Reopen so the next write starts from a clean, empty capture. */
    fclose(tf->file);
    tf->file = tmpfile();

    return n;
}

void summa_test_random_seed() {
    srand(time(0));
}

double summa_test_random_double_between(double min, double max) {
    double range = (max - min);
    double div   = RAND_MAX / range;
    return min + (rand() / div);
}

int64_t summa_test_random_integer_between(int64_t min, int64_t max) {
    uint64_t range = (uint64_t)max - (uint64_t)min;
    uint64_t bits  = ((uint64_t)rand() << 32) ^ (uint64_t)rand();
    uint64_t r     = (range == UINT64_MAX) ? bits : bits % (range + 1);
    return (int64_t)((uint64_t)min + r);
}

#endif /* SUMMA_TEST_IMPLEMENTATION_ONCE */
#endif /* SUMMA_TEST_IMPLEMENTATION */
