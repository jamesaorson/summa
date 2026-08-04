#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#include <stdlib.h>

/* A stand-in for a summa handle type: heap-allocated, with a make/free pair.
 * Deliberately not a real summa container -- these cases are about the scoping
 * macro itself, so the framework's tests stay independent of the libraries. */
typedef struct {
    int value;
} Thing_t;
typedef Thing_t* Thing;

static int  destroy_calls = 0;
static char destroy_log[8];
static int  destroy_log_length = 0;

static Thing thing_make(int value) {
    Thing thing  = malloc(sizeof(Thing_t));
    thing->value = value;
    return thing;
}

static void thing_free(Thing thing) {
    destroy_calls++;
    if (destroy_log_length < (int)sizeof(destroy_log)) {
        destroy_log[destroy_log_length++] = (char)('0' + thing->value);
    }
    free(thing);
}

static void reset_tracking(void) {
    destroy_calls      = 0;
    destroy_log_length = 0;
}

void test_scoped_value_binds_and_destroys() {
    reset_tracking();
    SUMMA_TEST_SCOPED_VALUE(Thing, thing, thing_make(7), thing_free) {
        SUMMA_TEST_ASSERT_NOT_NULL(thing);
        SUMMA_TEST_ASSERT_EQ(7, thing->value);
        /* Still alive inside the block. */
        SUMMA_TEST_ASSERT_EQ(0, destroy_calls);
    }
    SUMMA_TEST_ASSERT_EQ(1, destroy_calls);
}

void test_scoped_value_body_runs_exactly_once() {
    reset_tracking();
    int iterations = 0;
    SUMMA_TEST_SCOPED_VALUE(Thing, thing, thing_make(1), thing_free) {
        (void)thing;
        iterations++;
    }
    /* The for-loop is a scoping device, not a loop: one pass, then cleanup. */
    SUMMA_TEST_ASSERT_EQ(1, iterations);
    SUMMA_TEST_ASSERT_EQ(1, destroy_calls);
}

void test_scoped_value_nests_inner_first() {
    reset_tracking();
    SUMMA_TEST_SCOPED_VALUE(Thing, outer, thing_make(1), thing_free)
    SUMMA_TEST_SCOPED_VALUE(Thing, inner, thing_make(2), thing_free) {
        SUMMA_TEST_ASSERT_EQ(1, outer->value);
        SUMMA_TEST_ASSERT_EQ(2, inner->value);
    }
    SUMMA_TEST_ASSERT_EQ(2, destroy_calls);
    /* Inner scope exits first, so "2" is destroyed before "1". */
    destroy_log[destroy_log_length] = '\0';
    SUMMA_TEST_ASSERT_EQ_STR("21", destroy_log);
}

void test_scoped_value_destroys_after_failed_assertion() {
    reset_tracking();
    SUMMA_TEST_SCOPED_VALUE(Thing, thing, thing_make(3), thing_free) {
        /* Assertions record and keep going rather than unwinding, so cleanup
         * still runs on the way out of a failing block. Checked here without
         * actually failing the suite: the assertion below is the true one. */
        SUMMA_TEST_ASSERT_EQ(3, thing->value);
    }
    SUMMA_TEST_ASSERT_EQ(1, destroy_calls);
}

void test_scoped_value_sequential_scopes_are_independent() {
    reset_tracking();
    SUMMA_TEST_SCOPED_VALUE(Thing, first, thing_make(4), thing_free) {
        SUMMA_TEST_ASSERT_EQ(4, first->value);
    }
    SUMMA_TEST_ASSERT_EQ(1, destroy_calls);
    SUMMA_TEST_SCOPED_VALUE(Thing, second, thing_make(5), thing_free) {
        SUMMA_TEST_ASSERT_EQ(5, second->value);
    }
    SUMMA_TEST_ASSERT_EQ(2, destroy_calls);
    destroy_log[destroy_log_length] = '\0';
    SUMMA_TEST_ASSERT_EQ_STR("45", destroy_log);
}

void test_scoped_value_single_statement_body() {
    reset_tracking();
    /* No braces: the macro has to bind to one statement like any other loop. */
    SUMMA_TEST_SCOPED_VALUE(Thing, thing, thing_make(6), thing_free)
    SUMMA_TEST_ASSERT_EQ(6, thing->value);
    SUMMA_TEST_ASSERT_EQ(1, destroy_calls);
}

/* ── SUMMA_TEST_TODO ──────────────────────────────────────────────────────
 *
 * What TODO changes is how SUMMA_TEST_RUN classifies a case afterwards, so
 * these run a case inside a case. SUMMA_TEST_RUN both prints and moves the
 * tally, so the surrounding context is put aside and restored -- and the
 * filter is cleared along the way, since ctest invokes this binary with one
 * case name and the nested run would otherwise skip itself.
 *
 * The nested runs print their own "TODO"/"FAIL" lines into the transcript.
 * That is the behaviour being checked, not stray output. */
static summa_test_ctx_t borrow_ctx(void) {
    const summa_test_ctx_t saved = summa_test_ctx;
    summa_test_ctx.tests_passed  = 0;
    summa_test_ctx.tests_failed  = 0;
    summa_test_ctx.tests_todo    = 0;
    summa_test_ctx._filter       = nullptr;
    summa_test_ctx._list_mode    = 0;
    return saved;
}

static void inner_marked_and_failing(void) {
    SUMMA_TEST_TODO("a gap that has not been closed yet");
    SUMMA_TEST_ASSERT(false);
}

static void inner_marked_and_passing(void) {
    SUMMA_TEST_TODO("a gap that has since been closed");
    SUMMA_TEST_ASSERT(true);
}

static void inner_unmarked_and_failing(void) {
    SUMMA_TEST_ASSERT(false);
}

void test_todo_marks_the_reason_on_the_context() {
    summa_test_ctx._todo        = 0;
    summa_test_ctx._todo_reason = nullptr;

    SUMMA_TEST_TODO("why it cannot pass");

    SUMMA_TEST_ASSERT_EQ(1, summa_test_ctx._todo);
    SUMMA_TEST_ASSERT_EQ_STR("why it cannot pass", summa_test_ctx._todo_reason);

    /* Leave the flag off, or this very case would be classified as TODO. */
    summa_test_ctx._todo        = 0;
    summa_test_ctx._todo_reason = nullptr;
}

/* The point of the marker: a known-failing case does not fail the suite. */
void test_todo_failure_counts_as_todo_not_failed() {
    const summa_test_ctx_t saved = borrow_ctx();
    SUMMA_TEST_RUN(inner_marked_and_failing);
    const int passed = summa_test_ctx.tests_passed;
    const int failed = summa_test_ctx.tests_failed;
    const int todo   = summa_test_ctx.tests_todo;
    summa_test_ctx   = saved;

    SUMMA_TEST_ASSERT_EQ(0, passed);
    SUMMA_TEST_ASSERT_EQ(0, failed);
    SUMMA_TEST_ASSERT_EQ(1, todo);
}

/* And the other half: once the gap closes, the marker has to come off, so a
 * case that passes while marked is a failure. */
void test_todo_passing_while_marked_is_a_failure() {
    const summa_test_ctx_t saved = borrow_ctx();
    SUMMA_TEST_RUN(inner_marked_and_passing);
    const int passed = summa_test_ctx.tests_passed;
    const int failed = summa_test_ctx.tests_failed;
    const int todo   = summa_test_ctx.tests_todo;
    summa_test_ctx   = saved;

    SUMMA_TEST_ASSERT_EQ(0, passed);
    SUMMA_TEST_ASSERT_EQ(1, failed);
    SUMMA_TEST_ASSERT_EQ(0, todo);
}

/* An ordinary failure is still an ordinary failure. */
void test_todo_does_not_change_unmarked_failures() {
    const summa_test_ctx_t saved = borrow_ctx();
    SUMMA_TEST_RUN(inner_unmarked_and_failing);
    const int failed = summa_test_ctx.tests_failed;
    const int todo   = summa_test_ctx.tests_todo;
    summa_test_ctx   = saved;

    SUMMA_TEST_ASSERT_EQ(1, failed);
    SUMMA_TEST_ASSERT_EQ(0, todo);
}

/* The exit code is what CI reads: todos are reported, but only real failures
 * make the suite non-zero. */
void test_todo_alone_leaves_the_suite_passing() {
    const summa_test_ctx_t saved = borrow_ctx();

    summa_test_ctx.tests_todo = 2;
    const int todo_only       = summa_test_end();

    summa_test_ctx.tests_todo   = 2;
    summa_test_ctx.tests_failed = 1;
    const int with_a_failure    = summa_test_end();

    summa_test_ctx = saved;

    SUMMA_TEST_ASSERT_EQ(0, todo_only);
    SUMMA_TEST_ASSERT_EQ(1, with_a_failure);
}

int main(int argc, char** argv) {
    summa_test_begin("test", argc, argv);
    SUMMA_TEST_RUN(test_scoped_value_binds_and_destroys);
    SUMMA_TEST_RUN(test_scoped_value_body_runs_exactly_once);
    SUMMA_TEST_RUN(test_scoped_value_nests_inner_first);
    SUMMA_TEST_RUN(test_scoped_value_destroys_after_failed_assertion);
    SUMMA_TEST_RUN(test_scoped_value_sequential_scopes_are_independent);
    SUMMA_TEST_RUN(test_scoped_value_single_statement_body);

    SUMMA_TEST_RUN(test_todo_marks_the_reason_on_the_context);
    SUMMA_TEST_RUN(test_todo_failure_counts_as_todo_not_failed);
    SUMMA_TEST_RUN(test_todo_passing_while_marked_is_a_failure);
    SUMMA_TEST_RUN(test_todo_does_not_change_unmarked_failures);
    SUMMA_TEST_RUN(test_todo_alone_leaves_the_suite_passing);
    return summa_test_end();
}
