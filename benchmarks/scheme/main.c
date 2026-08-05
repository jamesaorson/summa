/* A benchmark for the Scheme evaluator's per-call cost.
 *
 * BUILTINS.md ends its tail-call section by naming three buys -- symbol
 * interning, arguments moved rather than copied, and a binding lookup that is
 * not a linear scan -- and then says all three "want a measurement first
 * rather than a guess". This is the measurement.
 *
 * It is not a test and deliberately does not live in tests/. A benchmark
 * asserts nothing; it prints numbers, and numbers that move between two builds
 * are the whole point. Putting it under ctest would buy a pass/fail that says
 * nothing and cost every CI run several seconds. It is built by the normal
 * build so it cannot rot, and run by hand -- `make benchmark` -- or not at all.
 *
 * ── What each case prices ─────────────────────────────────────────────────
 *
 * Cases are grouped by the cost they isolate, and the groups line up one to
 * one with the three follow-ups:
 *
 * - `call/…` -- frame allocation and binding. A tail loop over integers, then
 *   the same loop plus a nullary call, then plus a three-argument call. The
 *   deltas between the three are what a frame costs empty and what each
 *   binding adds on top.
 * - `args/…` -- the deep copy in `summa_scheme_procedure_frame`. One shape
 *   threads a list of fixed length through every iteration, at three lengths;
 *   the other walks a list to its end, at two lengths. Both are O(length) per
 *   call today, so per-call cost tracks the length. Moving arguments instead of
 *   copying them should change the *slope* of those lines, not their offset.
 * - `lookup/…` -- the linear `strcmp` walk. A loop under a deep chain of `let`
 *   frames, and a loop referencing a global defined after two hundred others,
 *   each paired with the shallow version of itself. The pair is the reading:
 *   the difference is the walk.
 * - `symbols/…` -- the string allocated per binding name. Twelve distinct,
 *   long-ish names bound per iteration, where the tail loop binds two short
 *   ones.
 *
 * Every case is a pair or a series for that reason. An absolute number here
 * describes this machine and this build; a ratio between two cases describes
 * the evaluator, and survives being read on a different one.
 *
 * ── What the columns mean ────────────────────────────────────────────────
 *
 * `calls` is the number of *user procedure* applications the program performs.
 * Builtin dispatches (`+`, `car`, ...) are not counted even though the
 * evaluator pays for them, so per-call figures are a stable denominator rather
 * than a true cost per application. Timing and allocation counts cover the
 * whole program -- reading it, building the global environment, and tearing it
 * down -- because a case's setup is a rounding error against its loop.
 *
 * `allocs/call` counts calls to malloc, calloc and realloc, through the shim
 * below; `bytes/call` is what those calls asked for. Both are worth having,
 * because they answer different questions. A name allocated per binding shows
 * up in the count. Copying a list of integers barely does -- the elements are
 * immediate values, so a deep copy of a hundred of them is two allocations and
 * a large memcpy -- and shows up in the bytes instead.
 *
 * Allocations that outlive a program are a leak rather than a benchmark result,
 * so they are reported on stderr and make the run exit non-zero. A clean run
 * says nothing about them, which is what keeps a diff quiet.
 *
 * One thing is deliberately allowed to outlive a program: the symbol intern
 * table, which is process-global and permanent by design. `bench_warm_symbols`
 * reads each case's source through once before the clock starts, so every name
 * the case mentions is already in the table -- which keeps that tally honest and
 * keeps the first measured run from being the one that pays for all of them.
 *
 * ── Reading a run ────────────────────────────────────────────────────────
 *
 * Output is one line per case in a fixed order, so two runs diff to the numbers
 * that moved and nothing else. Wall time will not diff to zero; allocation
 * counts will, since nothing in the evaluator is randomized and the collector's
 * threshold resets when a program's environment is torn down.
 */

/* Every standard header the summa headers reach for, pulled in ahead of the
 * allocator shim below. The shim is macros over malloc/calloc/realloc/free, and
 * a macro must not reach a *declaration* of the function it stands for -- so
 * everything that might declare one is already included by the time the macros
 * exist. */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma region Counting allocator

/* The evaluator allocates through plain malloc, so counting its allocations
 * means standing in front of that call. The summa headers are compiled into
 * this translation unit -- the benchmark is what defines
 * SUMMA_SCHEME_IMPLEMENTATION -- so redirecting the four allocator names here
 * redirects every allocation the evaluator makes, and nothing outside this
 * file is affected.
 *
 * The alternative was a platform allocation counter: mallinfo2 on glibc,
 * malloc_zone_statistics on Darwin. Two implementations of a counter, neither
 * portable, to learn what three `+= 1`s already know. */

static size_t BENCH_ALLOCATIONS = 0; /* malloc + calloc + realloc calls */
static size_t BENCH_BYTES       = 0; /* bytes those calls asked for */
static size_t BENCH_LIVE        = 0; /* allocations made and not yet released */

static void* bench_malloc(const size_t size) {
    BENCH_ALLOCATIONS++;
    BENCH_BYTES += size;
    BENCH_LIVE++;
    return malloc(size);
}

static void* bench_calloc(const size_t count, const size_t size) {
    BENCH_ALLOCATIONS++;
    BENCH_BYTES += count * size;
    BENCH_LIVE++;
    return calloc(count, size);
}

/* A realloc of a live block hands one back, so it is an allocation for the
 * count and for the bytes but not for the live tally. A realloc of null is a
 * malloc wearing another name, and counts as one. */
static void* bench_realloc(void* const pointer, const size_t size) {
    BENCH_ALLOCATIONS++;
    BENCH_BYTES += size;
    if (pointer == nullptr) {
        BENCH_LIVE++;
    }
    return realloc(pointer, size);
}

static void bench_free(void* const pointer) {
    if (pointer != nullptr) {
        BENCH_LIVE--;
    }
    free(pointer);
}

/* Function-like on purpose. An object-like `#define malloc bench_malloc` would
 * rewrite the bare token wherever it appeared -- including inside a
 * `__attribute__((malloc))` that some platform's headers might carry -- while
 * these expand only where a call is actually being made. */
#define malloc(size) bench_malloc(size)
#define calloc(count, size) bench_calloc(count, size)
#define realloc(pointer, size) bench_realloc(pointer, size)
#define free(pointer) bench_free(pointer)

#pragma endregion Counting allocator

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#pragma region Program text

/* Cases are parameterized -- a list length, a chain depth, a count of globals
 * -- so their programs are generated rather than written out. 256 KiB holds
 * the largest of them (two hundred and fifty six `define`s) several times
 * over. */
#define BENCH_PROGRAM_CAPACITY (1u << 18)

typedef struct {
    char   text[BENCH_PROGRAM_CAPACITY];
    size_t length;
} BenchProgram;

static const char* BENCH_NAME = "benchmarks.scheme";

static void bench_program_reset(BenchProgram* const program) {
    program->length  = 0;
    program->text[0] = '\0';
}

static void bench_program_append(BenchProgram* const program, const char* const format, ...) {
    const size_t remaining = BENCH_PROGRAM_CAPACITY - program->length;

    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(program->text + program->length, remaining, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= remaining) {
        fprintf(stderr,
                "%s: generated program outgrew its %u byte buffer.\n"
                "Lower the case's size, or raise BENCH_PROGRAM_CAPACITY in benchmarks/scheme/main.c.\n",
                BENCH_NAME,
                BENCH_PROGRAM_CAPACITY);
        exit(EXIT_FAILURE);
    }
    program->length += (size_t)written;
}

/* `(define <name> (quote (1 2 ... size)))`. Quoted rather than built with
 * `cons`, so the list costs one copy at read time instead of a quadratic
 * construction the case never meant to measure. */
static void bench_append_list_definition(BenchProgram* const program, const char* const name, const int64_t size) {
    bench_program_append(program, "(define %s (quote (", name);
    for (int64_t i = 1; i <= size; i++) {
        bench_program_append(program, "%s%" PRId64, i == 1 ? "" : " ", i);
    }
    bench_program_append(program, ")))");
}

#pragma endregion Program text

#pragma region Cases

/* Fills `program` and returns how many user-procedure applications running it
 * will perform. `iterations` is already scaled; `size` is the case's own
 * parameter and is zero for cases that have none. */
typedef int64_t (*BenchBuildFn)(BenchProgram* program, int64_t iterations, int64_t size);

typedef struct {
    const char*  name;
    const char*  isolates;
    BenchBuildFn build;
    int64_t      iterations; /* at scale 1 */
    int64_t      size;
} BenchCase;

/* ── call/… : what a frame and its bindings cost ───────────────────────── */

static int64_t
bench_build_tail_loop(BenchProgram* const program, const int64_t iterations, [[maybe_unused]] const int64_t size) {
    bench_program_append(program,
                         "(define (loop i acc) (if (= i 0) acc (loop (- i 1) (+ acc i))))"
                         "(loop %" PRId64 " 0)",
                         iterations);
    return iterations;
}

static int64_t
bench_build_nullary(BenchProgram* const program, const int64_t iterations, [[maybe_unused]] const int64_t size) {
    bench_program_append(program,
                         "(define (unit) 1)"
                         "(define (loop i acc) (if (= i 0) acc (loop (- i 1) (+ acc (unit)))))"
                         "(loop %" PRId64 " 0)",
                         iterations);
    /* Two applications an iteration: the loop and `unit`. */
    return iterations * 2;
}

static int64_t
bench_build_ternary(BenchProgram* const program, const int64_t iterations, [[maybe_unused]] const int64_t size) {
    bench_program_append(program,
                         "(define (three a b c) a)"
                         "(define (loop i acc) (if (= i 0) acc (loop (- i 1) (+ acc (three 1 2 3)))))"
                         "(loop %" PRId64 " 0)",
                         iterations);
    return iterations * 2;
}

/* ── args/… : what copying an argument costs ───────────────────────────── */

/* The same list handed to the same parameter every iteration, never taken
 * apart. Nothing about the program is O(length), and since #40 nothing the
 * evaluator does to run it is either: the argument is moved into the frame and
 * the reference that produced it is a counter increment. This series is what
 * says so -- ns/call should be flat across the three sizes, and is. */
static int64_t bench_build_list_thread(BenchProgram* const program, const int64_t iterations, const int64_t size) {
    bench_append_list_definition(program, "xs", size);
    bench_program_append(program,
                         "(define (loop i xs acc) (if (= i 0) acc (loop (- i 1) xs (+ acc (car xs)))))"
                         "(loop %" PRId64 " xs 0)",
                         iterations);
    return iterations;
}

/* A list walked to its end, `iterations` times over. The walk is written the
 * way Scheme wants it written -- tail recursive, one element per call -- so
 * nothing here is quadratic except what `cdr` does. Doubling `size` while
 * halving `iterations` keeps the call count fixed, which is what makes the two
 * list-walk lines directly comparable.
 *
 * Unlike list-thread this one cannot go flat, and the reason is the language
 * rather than the evaluator: `cdr` builds a new list of length n-1, so walking
 * a list of n copies n(n-1)/2 elements however cheap a reference is. Cons cells
 * are what would fix it. */
static int64_t bench_build_list_walk(BenchProgram* const program, const int64_t iterations, const int64_t size) {
    bench_append_list_definition(program, "xs", size);
    bench_program_append(program,
                         "(define (walk l acc) (if (null? l) acc (walk (cdr l) (+ acc (car l)))))"
                         "(define (repeat k acc) (if (= k 0) acc (repeat (- k 1) (walk xs 0))))"
                         "(repeat %" PRId64 " 0)",
                         iterations);
    /* One `repeat` and one `walk` per element, plus the walk's base case. */
    return iterations * (size + 2);
}

/* ── lookup/… : what the chain walk costs ──────────────────────────────── */

/* `size` nested `let` frames, and the loop at the bottom of them. The loop
 * body names `outermost` -- bound by the first `let`, so found last -- and
 * three builtins that live past every frame in the chain, in the global
 * environment. Depth 1 is the same program with the chain removed. */
static int64_t bench_build_deep_chain(BenchProgram* const program, const int64_t iterations, const int64_t size) {
    bench_program_append(program, "(let ((outermost 1))");
    for (int64_t i = 1; i < size; i++) {
        bench_program_append(program, "(let ((filler%" PRId64 " %" PRId64 "))", i, i);
    }
    bench_program_append(program,
                         "(letrec ((loop (lambda (i acc)"
                         "  (if (= i 0) acc (loop (- i 1) (+ acc outermost))))))"
                         "  (loop %" PRId64 " 0))",
                         iterations);
    for (int64_t i = 0; i < size; i++) {
        bench_program_append(program, ")");
    }
    return iterations;
}

/* `size` globals defined ahead of the loop, which then names the last of them.
 * The global frame is scanned front to back, and it already holds twenty-three
 * builtins, so this prices the frame every program grows and no program can
 * avoid. */
static int64_t bench_build_globals(BenchProgram* const program, const int64_t iterations, const int64_t size) {
    for (int64_t i = 1; i <= size; i++) {
        bench_program_append(program, "(define global-%" PRId64 " %" PRId64 ")", i, i);
    }
    bench_program_append(program,
                         "(define (loop i acc) (if (= i 0) acc (loop (- i 1) (+ acc global-%" PRId64 "))))"
                         "(loop %" PRId64 " 0)",
                         size,
                         iterations);
    return iterations;
}

/* ── symbols/… : what a name costs ─────────────────────────────────────── */

/* Twelve distinct names bound per iteration, none of them one character long,
 * against the tail loop's two. Every one of them allocates a SummaString whose
 * contents were already in the source text, and every reference to one walks
 * the frame comparing characters. */
static int64_t
bench_build_wide_frame(BenchProgram* const program, const int64_t iterations, [[maybe_unused]] const int64_t size) {
    bench_program_append(program,
                         "(define (accumulate alpha beta gamma delta epsilon zeta eta theta)"
                         "  (let* ((first-partial (+ alpha beta))"
                         "         (second-partial (+ gamma delta))"
                         "         (third-partial (+ epsilon zeta))"
                         "         (fourth-partial (+ eta theta)))"
                         "    (+ first-partial second-partial third-partial fourth-partial)))"
                         "(define (loop i acc) (if (= i 0) acc (loop (- i 1) (accumulate 1 2 3 4 5 6 7 8))))"
                         "(loop %" PRId64 " 0)",
                         iterations);
    return iterations * 2;
}

/* The order here is the order they print in, and it is the order they should be
 * read in: each group opens with the shallow or empty version of itself, so the
 * lines below it have something to be a difference from. */
static const BenchCase BENCH_CASES[] = {
    {
        .name       = "call/tail-loop",
        .isolates   = "frame + 2 bindings per call, integer arithmetic only",
        .build      = bench_build_tail_loop,
        .iterations = 200000,
        .size       = 0,
    },
    {
        .name       = "call/nullary",
        .isolates   = "the tail loop plus a call binding nothing -- an empty frame",
        .build      = bench_build_nullary,
        .iterations = 200000,
        .size       = 0,
    },
    {
        .name       = "call/ternary",
        .isolates   = "the same, binding three arguments -- the delta is per binding",
        .build      = bench_build_ternary,
        .iterations = 200000,
        .size       = 0,
    },
    {
        .name       = "args/list-thread-1",
        .isolates   = "a 1-element list threaded through every call",
        .build      = bench_build_list_thread,
        .iterations = 100000,
        .size       = 1,
    },
    {
        .name       = "args/list-thread-16",
        .isolates   = "16 elements -- the same program, more to copy",
        .build      = bench_build_list_thread,
        .iterations = 100000,
        .size       = 16,
    },
    {
        .name       = "args/list-thread-128",
        .isolates   = "128 elements -- ns/call should be flat here and is not",
        .build      = bench_build_list_thread,
        .iterations = 100000,
        .size       = 128,
    },
    {
        .name       = "args/list-walk-32",
        .isolates   = "walking a 32-element list, tail recursively",
        .build      = bench_build_list_walk,
        .iterations = 4000,
        .size       = 32,
    },
    {
        .name       = "args/list-walk-256",
        .isolates   = "the same walk over 256 elements, same call count",
        .build      = bench_build_list_walk,
        .iterations = 500,
        .size       = 256,
    },
    {
        .name       = "lookup/chain-1",
        .isolates   = "a loop one frame under the globals",
        .build      = bench_build_deep_chain,
        .iterations = 100000,
        .size       = 1,
    },
    {
        .name       = "lookup/chain-32",
        .isolates   = "the same loop under 32 let frames -- the difference is the walk",
        .build      = bench_build_deep_chain,
        .iterations = 100000,
        .size       = 32,
    },
    {
        .name       = "lookup/globals-8",
        .isolates   = "a loop naming the last of 8 globals, past 23 builtins",
        .build      = bench_build_globals,
        .iterations = 100000,
        .size       = 8,
    },
    {
        .name       = "lookup/globals-256",
        .isolates   = "the last of 256 globals -- the global frame is scanned front to back",
        .build      = bench_build_globals,
        .iterations = 100000,
        .size       = 256,
    },
    {
        .name       = "symbols/wide-frame",
        .isolates   = "12 distinct multi-character names bound per iteration",
        .build      = bench_build_wide_frame,
        .iterations = 100000,
        .size       = 0,
    },
};

#define BENCH_CASE_COUNT (sizeof(BENCH_CASES) / sizeof(BENCH_CASES[0]))

/* A null filter selects everything; otherwise a case is selected by name
 * prefix, so `-f args` takes the group and `-f args/list-walk-32` takes one. */
static bool bench_selected(const BenchCase* const bench_case, const char* const filter) {
    return filter == nullptr || strncmp(bench_case->name, filter, strlen(filter)) == 0;
}

static size_t bench_selected_count(const char* const filter) {
    size_t selected = 0;
    for (size_t i = 0; i < BENCH_CASE_COUNT; i++) {
        selected += bench_selected(&BENCH_CASES[i], filter) ? 1 : 0;
    }
    return selected;
}

#pragma endregion Cases

#pragma region Running

/* Wall clock through C11's timespec_get -- the one clock the standard library
 * offers, and enough for a case that runs for milliseconds. */
static double bench_seconds(void) {
    struct timespec now = {};
    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        fprintf(stderr, "%s: timespec_get(TIME_UTC) failed -- no clock to measure with.\n", BENCH_NAME);
        exit(EXIT_FAILURE);
    }
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

typedef struct {
    double seconds;
    size_t allocations;
    size_t bytes;
    long   net; /* allocations the program did not release; expected zero */
} BenchResult;

/* Read, evaluate, discard, repeat -- the driver loop from the test suites. */
static SummaSchemeError bench_run_program(const SummaSchemeEnvironment env, const char* const program) {
    SummaSchemeError err    = summa_success();
    const char*      cursor = program;
    SummaSchemeValue result = {};

    while (*cursor) {
        SummaSchemeValue form = {};

        err = summa_scheme_read(env, cursor, &cursor, &form);
        if (err.had) {
            break;
        }

        summa_scheme_value_free(&result);
        result = (SummaSchemeValue){};
        err    = summa_scheme_evaluate(env, form, &result);
        summa_scheme_value_free(&form);
        if (err.had) {
            break;
        }
    }

    summa_scheme_value_free(&result);
    return err;
}

/* Reads the program through once and throws the result away, which puts every
 * name it mentions into the process-wide intern table. Two reasons, and the
 * second is the load-bearing one:
 *
 * - A name is interned once and reused for the life of the process, so the
 *   *first* run of a case would otherwise carry allocations no later run makes
 *   -- a per-call cost that is really a per-process one.
 * - Those allocations are never released, by design, so they would read as a
 *   leak in the tally below. The intern table is the one thing here that is
 *   *supposed* to outlive a program, and this is what keeps the leak check
 *   able to say so.
 *
 * A read error is ignored: the measured run makes the same mistake a moment
 * later and reports it properly. */
static void bench_warm_symbols(const char* const program) {
    const SummaSchemeEnvironment env    = summa_scheme_environment_make_global();
    const char*                  cursor = program;

    while (*cursor) {
        SummaSchemeValue form = {};
        if (summa_scheme_read(env, cursor, &cursor, &form).had) {
            break;
        }
        summa_scheme_value_free(&form);
    }

    summa_scheme_environment_free(env);
}

/* One run of one case: a fresh global environment, the program, and the
 * teardown. All three are inside the measurement, since the environment a
 * program leaves behind is part of what it cost. */
static BenchResult bench_run_once(const char* const name, const char* const program) {
    const size_t allocations_before = BENCH_ALLOCATIONS;
    const size_t bytes_before       = BENCH_BYTES;
    const size_t live_before        = BENCH_LIVE;
    const double started            = bench_seconds();

    const SummaSchemeEnvironment env = summa_scheme_environment_make_global();
    const SummaSchemeError       err = bench_run_program(env, program);
    summa_scheme_environment_free(env);

    const double elapsed = bench_seconds() - started;

    if (err.had) {
        fprintf(stderr,
                "%s: case '%s' failed to run: %s\n"
                "The case's program is generated in benchmarks/scheme/main.c -- either it names something the "
                "evaluator no longer has, or this is an evaluator bug worth a test rather than a benchmark.\n",
                BENCH_NAME,
                name,
                err.message);
        exit(EXIT_FAILURE);
    }

    return (BenchResult){
        .seconds     = elapsed,
        .allocations = BENCH_ALLOCATIONS - allocations_before,
        .bytes       = BENCH_BYTES - bytes_before,
        .net         = (long)BENCH_LIVE - (long)live_before,
    };
}

#pragma endregion Running

#pragma region Command line

typedef struct {
    const char* filter;
    int64_t     scale;
    int64_t     repeat;
    bool        list;
} BenchOptions;

static void bench_print_usage(FILE* const out) {
    fprintf(out,
            "usage: %s [options]\n"
            "\n"
            "Measures the Scheme evaluator's per-call cost. With no options it runs every\n"
            "case once at scale 1, which takes a few seconds.\n"
            "\n"
            "  -h, --help           show this and exit\n"
            "  -l, --list           list the cases and the cost each one prices\n"
            "  -f, --filter PREFIX  run only cases whose name starts with PREFIX\n"
            "                       (the groups are call/, args/, lookup/, symbols/)\n"
            "  -s, --scale N        multiply every case's iteration count by N (default 1)\n"
            "  -r, --repeat N       run each case N times, report the fastest (default 1)\n"
            "\n"
            "Compare two evaluators by running this on each and diffing the output: the\n"
            "case order is fixed and allocation counts are exact, so only what moved moves.\n"
            "Ratios between cases in one run travel between machines; absolute times do\n"
            "not.\n",
            BENCH_NAME);
}

/* The call count is whatever the builder says it is, so the listing builds each
 * program and throws it away rather than duplicating the arithmetic. */
static void bench_print_cases(FILE* const out) {
    BenchProgram* const program = malloc(sizeof(BenchProgram));
    if (program == nullptr) {
        fprintf(stderr, "%s: out of memory building the case list.\n", BENCH_NAME);
        exit(EXIT_FAILURE);
    }

    fprintf(out, "%-22s %10s  %s\n", "case", "calls", "prices");
    for (size_t i = 0; i < BENCH_CASE_COUNT; i++) {
        const BenchCase* const bench_case = &BENCH_CASES[i];
        bench_program_reset(program);
        const int64_t calls = bench_case->build(program, bench_case->iterations, bench_case->size);
        fprintf(out, "%-22s %10" PRId64 "  %s\n", bench_case->name, calls, bench_case->isolates);
    }

    free(program);
}

/* Parses a positive count, or explains what was wrong with it and stops. */
static int64_t bench_parse_count(const char* const option, const char* const text) {
    char* end              = nullptr;
    errno                  = 0;
    const long long parsed = strtoll(text, &end, 10);

    if (end == text || *end != '\0' || parsed <= 0 || errno != 0) {
        fprintf(stderr,
                "%s: %s wants a positive whole number, got '%s'.\n"
                "Try `%s %s 4`.\n",
                BENCH_NAME,
                option,
                text,
                BENCH_NAME,
                option);
        exit(EXIT_FAILURE);
    }
    return (int64_t)parsed;
}

/* Returns the value of an option that takes one, whether it arrived as
 * `--scale 4` or `--scale=4`. */
static const char* bench_option_value(
    const char* const option, const char* const inline_value, const int argc, char** const argv, int* const index) {
    if (inline_value != nullptr) {
        return inline_value;
    }
    if (*index + 1 >= argc) {
        fprintf(stderr,
                "%s: %s needs a value.\n"
                "Try `%s %s 4`, or `%s --help` for the option list.\n",
                BENCH_NAME,
                option,
                BENCH_NAME,
                option,
                BENCH_NAME);
        exit(EXIT_FAILURE);
    }
    *index += 1;
    return argv[*index];
}

static BenchOptions bench_parse_options(const int argc, char** const argv) {
    BenchOptions options = {.filter = nullptr, .scale = 1, .repeat = 1, .list = false};

    for (int i = 1; i < argc; i++) {
        char* const argument = argv[i];
        /* `--scale=4` and `--scale 4` are the same option; split the first
         * shape into the second before matching. */
        char*       equals = strchr(argument, '=');
        const char* value  = nullptr;
        if (equals != nullptr && argument[0] == '-') {
            *equals = '\0';
            value   = equals + 1;
        }

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            bench_print_usage(stdout);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argument, "-l") == 0 || strcmp(argument, "--list") == 0) {
            options.list = true;
        } else if (strcmp(argument, "-f") == 0 || strcmp(argument, "--filter") == 0) {
            options.filter = bench_option_value(argument, value, argc, argv, &i);
        } else if (strcmp(argument, "-s") == 0 || strcmp(argument, "--scale") == 0) {
            options.scale = bench_parse_count(argument, bench_option_value(argument, value, argc, argv, &i));
        } else if (strcmp(argument, "-r") == 0 || strcmp(argument, "--repeat") == 0) {
            options.repeat = bench_parse_count(argument, bench_option_value(argument, value, argc, argv, &i));
        } else {
            fprintf(stderr,
                    "%s: unknown option '%s'.\n"
                    "Run `%s --help` for the option list, or `%s --list` for the case names.\n",
                    BENCH_NAME,
                    argument,
                    BENCH_NAME,
                    BENCH_NAME);
            exit(EXIT_FAILURE);
        }
    }

    return options;
}

#pragma endregion Command line

int main(const int argc, char** const argv) {
    const BenchOptions options = bench_parse_options(argc, argv);

    if (options.list) {
        bench_print_cases(stdout);
        return EXIT_SUCCESS;
    }

    /* Checked before anything is printed, so a mistyped filter answers with the
     * fix rather than with an empty table under a preamble. */
    if (bench_selected_count(options.filter) == 0) {
        fprintf(stderr,
                "%s: no case name starts with '%s'.\n"
                "Run `%s --list` to see the case names; the groups are call/, args/, lookup/ and symbols/.\n",
                BENCH_NAME,
                options.filter,
                BENCH_NAME);
        return EXIT_FAILURE;
    }

    if (argc == 1) {
        printf("No options given, so this is every case once at scale 1. `%s --help` has the rest.\n\n", BENCH_NAME);
    }

#ifndef NDEBUG
    printf("Note: NDEBUG is not set, so this is an unoptimized build and these numbers are several times slower\n"
           "      than the evaluator really is. Two such builds still compare honestly; for headline figures run\n"
           "      `make benchmark CMAKE_BUILD_TYPE=Release`.\n\n");
#endif

    /* Built on the heap: BenchProgram is a quarter of a megabyte, which is more
     * than some platforms give a thread's stack. */
    BenchProgram* const program = malloc(sizeof(BenchProgram));
    if (program == nullptr) {
        fprintf(stderr, "%s: out of memory allocating the program buffer.\n", BENCH_NAME);
        return EXIT_FAILURE;
    }

    printf("scale %" PRId64 ", best of %" PRId64 "\n\n", options.scale, options.repeat);
    printf("%-22s %10s %9s %9s %10s %10s\n", "case", "calls", "ms", "ns/call", "allocs/call", "bytes/call");

    bool leaked = false;
    for (size_t i = 0; i < BENCH_CASE_COUNT; i++) {
        const BenchCase* const bench_case = &BENCH_CASES[i];
        if (!bench_selected(bench_case, options.filter)) {
            continue;
        }

        bench_program_reset(program);
        const int64_t calls = bench_case->build(program, bench_case->iterations * options.scale, bench_case->size);

        bench_warm_symbols(program->text);

        BenchResult best = {};
        for (int64_t attempt = 0; attempt < options.repeat; attempt++) {
            const BenchResult result = bench_run_once(bench_case->name, program->text);
            if (attempt == 0 || result.seconds < best.seconds) {
                best = result;
            }
        }

        printf("%-22s %10" PRId64 " %9.1f %9.1f %10.2f %10.1f\n",
               bench_case->name,
               calls,
               best.seconds * 1e3,
               best.seconds * 1e9 / (double)calls,
               (double)best.allocations / (double)calls,
               (double)best.bytes / (double)calls);

        /* Off the table on purpose: a clean run prints nothing here, so this
         * line only ever appears in a diff because something regressed. */
        if (best.net != 0) {
            fprintf(stderr,
                    "%s: case '%s' ended with %ld allocations still live.\n"
                    "That is a leak in the evaluator, not a benchmark result -- the case's timings mean nothing until "
                    "it is fixed. `leaks --atExit -- ./build/debug/tests/tests.scheme.lifetime` is where to start.\n",
                    BENCH_NAME,
                    bench_case->name,
                    best.net);
            leaked = true;
        }
    }

    free(program);

    return leaked ? EXIT_FAILURE : EXIT_SUCCESS;
}
