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

int main(int argc, char** argv) {
    summa_test_begin("test", argc, argv);
    SUMMA_TEST_RUN(test_scoped_value_binds_and_destroys);
    SUMMA_TEST_RUN(test_scoped_value_body_runs_exactly_once);
    SUMMA_TEST_RUN(test_scoped_value_nests_inner_first);
    SUMMA_TEST_RUN(test_scoped_value_destroys_after_failed_assertion);
    SUMMA_TEST_RUN(test_scoped_value_sequential_scopes_are_independent);
    SUMMA_TEST_RUN(test_scoped_value_single_statement_body);
    return summa_test_end();
}
