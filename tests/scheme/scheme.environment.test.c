#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#define SCOPED_ENV(var)      \
    SUMMA_TEST_SCOPED_VALUE( \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_empty(), summa_scheme_environment_free)

/* No scope around either of these, and that is the point of interning: a name
 * belongs to the process-wide table for as long as the process lives, so there
 * is no allocation here for a scope to release. set() takes ownership of the
 * binding's value and of nothing else. */
static void bind_integer(SummaSchemeEnvironment env, const char* name, int64_t value) {
    summa_scheme_environment_set(
        env, summa_scheme_binding_make(summa_scheme_symbol_intern(name), summa_make_scheme_integer(value)));
}

static SummaSchemeSymbol symbol_of(const char* name) {
    return (SummaSchemeSymbol){.value = summa_scheme_symbol_intern(name)};
}

/* Reads and evaluates one expression against `env`. The frames a program makes
 * are not reachable from outside the evaluator, but a closure's is: a procedure
 * holds the environment it was defined in, so returning a `lambda` from a call
 * hands the call's own frame back to be measured. */
static SummaSchemeError evaluate_source(const SummaSchemeEnvironment env, const char* source, SummaSchemeValue* out) {
    SummaSchemeValue form   = {};
    const char*      cursor = source;
    SummaSchemeError err    = summa_scheme_read(env, source, &cursor, &form);
    if (!err.had) {
        err = summa_scheme_evaluate(env, form, out);
        summa_scheme_value_free(&form);
    }
    return err;
}

void test_environment_make_empty_starts_unbound() {
    SCOPED_ENV(env) {
        SUMMA_TEST_ASSERT_NOT_NULL(env);
        SUMMA_TEST_ASSERT_EQ(0u, env->bindings.length);
        SUMMA_TEST_ASSERT_NULL(env->parent);
    }
}

void test_environment_survives_the_block_that_made_it() {
    /* A heap environment outlives its constructing scope. The compound-literal
     * version this replaced could not: the pointer dangled the moment the
     * enclosing block ended, which is exactly what a captured environment
     * cannot tolerate. */
    SummaSchemeEnvironment env = nullptr;
    {
        env = summa_scheme_environment_make_empty();
        bind_integer(env, "x", 1);
    }
    SUMMA_TEST_ASSERT_NOT_NULL(env);
    SUMMA_TEST_ASSERT_EQ(1u, env->bindings.length);
    summa_scheme_environment_free(env);
}

void test_environment_set_then_get() {
    SCOPED_ENV(env) {
        bind_integer(env, "x", 42);
        SUMMA_TEST_ASSERT_EQ(1u, env->bindings.length);

        SummaSchemeBinding out   = {};
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of("x"), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(42, out.value.value.integer.value);
    }
}

void test_environment_set_existing_name_rebinds_in_place() {
    SCOPED_ENV(env) {
        bind_integer(env, "x", 1);
        bind_integer(env, "x", 2);

        /* Rebinding replaces rather than appends. The by-value copy this used
         * to make meant the second set updated a temporary and the environment
         * kept reporting 1. */
        SUMMA_TEST_ASSERT_EQ(1u, env->bindings.length);

        SummaSchemeBinding out   = {};
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of("x"), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(2, out.value.value.integer.value);
    }
}

void test_environment_rebinding_releases_the_previous_value() {
    SCOPED_ENV(env) {
        /* The displaced string value has to be freed by set(), not orphaned --
         * a leak the leaks(1) run over this binary would catch. */
        summa_scheme_environment_set(
            env, summa_scheme_binding_make(summa_scheme_symbol_intern("s"), summa_make_scheme_string("first")));
        summa_scheme_environment_set(
            env, summa_scheme_binding_make(summa_scheme_symbol_intern("s"), summa_make_scheme_string("second")));

        SUMMA_TEST_ASSERT_EQ(1u, env->bindings.length);

        SummaSchemeBinding out   = {};
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of("s"), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ_STR("second", out.value.value.string.value->value);
    }
}

void test_environment_distinct_names_both_bound() {
    SCOPED_ENV(env) {
        bind_integer(env, "x", 1);
        bind_integer(env, "y", 2);
        SUMMA_TEST_ASSERT_EQ(2u, env->bindings.length);

        SummaSchemeBinding out = {};
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(env, symbol_of("x"), &out).had);
        SUMMA_TEST_ASSERT_EQ(1, out.value.value.integer.value);
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(env, symbol_of("y"), &out).had);
        SUMMA_TEST_ASSERT_EQ(2, out.value.value.integer.value);
    }
}

void test_environment_get_unbound_name_errors() {
    SCOPED_ENV(env) {
        SummaSchemeBinding out   = {};
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of("nope"), &out);
        SUMMA_TEST_ASSERT(error.had);
        SUMMA_TEST_ASSERT_EQ_STR("Unbound variable: nope", error.message);
    }
}

void test_environment_get_falls_through_to_parent() {
    SCOPED_ENV(parent) {
        bind_integer(parent, "outer", 7);

        SummaSchemeEnvironment child = summa_scheme_environment_make(parent);
        SUMMA_TEST_ASSERT_EQ(parent, child->parent);

        SummaSchemeBinding out   = {};
        SummaSchemeError   error = summa_scheme_environment_get(child, symbol_of("outer"), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(7, out.value.value.integer.value);

        summa_scheme_environment_free(child);
    }
}

void test_environment_child_binding_shadows_parent() {
    SCOPED_ENV(parent) {
        bind_integer(parent, "x", 1);

        SummaSchemeEnvironment child = summa_scheme_environment_make(parent);
        bind_integer(child, "x", 2);

        SummaSchemeBinding out = {};
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(child, symbol_of("x"), &out).had);
        SUMMA_TEST_ASSERT_EQ(2, out.value.value.integer.value);
        /* The parent's own binding is untouched by the shadow. */
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(parent, symbol_of("x"), &out).had);
        SUMMA_TEST_ASSERT_EQ(1, out.value.value.integer.value);

        summa_scheme_environment_free(child);
    }
}

void test_environment_freeing_child_leaves_parent_alive() {
    SCOPED_ENV(parent) {
        bind_integer(parent, "x", 1);

        SummaSchemeEnvironment child = summa_scheme_environment_make(parent);
        summa_scheme_environment_free(child);

        /* parent is borrowed by the child, never owned, so it is still usable
         * here. The reverse -- a parent outliving its children -- is the
         * constraint callers currently have to honour by hand. */

        SummaSchemeBinding out = {};
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(parent, symbol_of("x"), &out).had);
        SUMMA_TEST_ASSERT_EQ(1, out.value.value.integer.value);
    }
}

#pragma region Binding storage is sized to the frame

void test_environment_make_empty_allocates_no_binding_storage() {
    /* A frame that binds nothing costs its header and nothing else. This used
     * to be eight SummaSchemeBinding slots -- 384 bytes -- for every
     * environment ever made, whether or not anything went in it. */
    SCOPED_ENV(env) {
        SUMMA_TEST_ASSERT_EQ(0u, env->bindings.capacity);
        SUMMA_TEST_ASSERT_NULL(env->bindings.value);
    }
}

void test_environment_make_with_capacity_reserves_exactly() {
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeEnvironment,
                            env,
                            summa_scheme_environment_make_with_capacity(nullptr, 3),
                            summa_scheme_environment_free) {
        SUMMA_TEST_ASSERT_EQ(3u, env->bindings.capacity);
        SUMMA_TEST_ASSERT_EQ(0u, env->bindings.length);
        bind_integer(env, "a", 1);
        bind_integer(env, "b", 2);
        bind_integer(env, "c", 3);
        /* Three bindings into three slots: no growth, no slack. */
        SUMMA_TEST_ASSERT_EQ(3u, env->bindings.capacity);
        SUMMA_TEST_ASSERT_EQ(3u, env->bindings.length);
    }
}

void test_environment_grows_past_the_capacity_it_was_given() {
    /* Right-sizing must not cost the ability to grow -- a body with an internal
     * `define` binds more than the call's arity said it would. */
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeEnvironment,
                            env,
                            summa_scheme_environment_make_with_capacity(nullptr, 1),
                            summa_scheme_environment_free) {
        char name[8] = {};
        for (int i = 0; i < 12; i++) {
            snprintf(name, sizeof(name), "v%d", i);
            bind_integer(env, name, i);
        }
        SUMMA_TEST_ASSERT_EQ(12u, env->bindings.length);
        SUMMA_TEST_ASSERT(env->bindings.capacity >= 12u);
        for (int i = 0; i < 12; i++) {
            snprintf(name, sizeof(name), "v%d", i);
            SummaSchemeBinding out = {};
            SUMMA_TEST_ASSERT(!summa_scheme_environment_get(env, symbol_of(name), &out).had);
            SUMMA_TEST_ASSERT_EQ(i, out.value.value.integer.value);
        }
    }
}

void test_environment_grows_from_zero_capacity() {
    SCOPED_ENV(env) {
        bind_integer(env, "x", 5);
        SUMMA_TEST_ASSERT_EQ(1u, env->bindings.length);
        SUMMA_TEST_ASSERT(env->bindings.capacity >= 1u);
        SummaSchemeBinding out = {};
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(env, symbol_of("x"), &out).had);
        SUMMA_TEST_ASSERT_EQ(5, out.value.value.integer.value);
    }
}

void test_environment_reserve_only_grows() {
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeEnvironment,
                            env,
                            summa_scheme_environment_make_with_capacity(nullptr, 4),
                            summa_scheme_environment_free) {
        summa_scheme_environment_reserve(env, 1);
        SUMMA_TEST_ASSERT_EQ(4u, env->bindings.capacity);
        summa_scheme_environment_reserve(env, 9);
        SUMMA_TEST_ASSERT_EQ(9u, env->bindings.capacity);
    }
}

void test_environment_global_frame_is_sized_to_the_builtins() {
    SUMMA_TEST_SCOPED_VALUE(
        SummaSchemeEnvironment, env, summa_scheme_environment_make_global(), summa_scheme_environment_free) {
        /* Every builtin is bound, and nothing was doubled past them on the way
         * -- the global frame is filled by a loop that knows its own length. */
        SUMMA_TEST_ASSERT_EQ(env->bindings.length, env->bindings.capacity);
    }
}

void test_environment_call_frame_is_sized_to_the_arity() {
    SUMMA_TEST_SCOPED_VALUE(
        SummaSchemeEnvironment, global, summa_scheme_environment_make_global(), summa_scheme_environment_free) {
        SummaSchemeValue closure = {};
        /* The returned procedure closes over the frame the call made, so its
         * `closure` is that frame: two parameters, two slots. */
        const SummaSchemeError err = evaluate_source(global, "((lambda (a b) (lambda () (+ a b))) 1 2)", &closure);
        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            SUMMA_TEST_ASSERT_EQ(SummaSchemeProcedureType, closure.type);
            const SummaSchemeEnvironment frame = closure.value.procedure.closure;
            SUMMA_TEST_ASSERT_NOT_NULL(frame);
            if (frame) {
                SUMMA_TEST_ASSERT_EQ(2u, frame->bindings.length);
                SUMMA_TEST_ASSERT_EQ(2u, frame->bindings.capacity);
            }
        }
        summa_scheme_value_free(&closure);
    }
}

void test_environment_nullary_call_frame_holds_no_binding_storage() {
    SUMMA_TEST_SCOPED_VALUE(
        SummaSchemeEnvironment, global, summa_scheme_environment_make_global(), summa_scheme_environment_free) {
        SummaSchemeValue       closure = {};
        const SummaSchemeError err     = evaluate_source(global, "((lambda () (lambda () 1)))", &closure);
        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            const SummaSchemeEnvironment frame = closure.value.procedure.closure;
            SUMMA_TEST_ASSERT_NOT_NULL(frame);
            if (frame) {
                SUMMA_TEST_ASSERT_EQ(0u, frame->bindings.capacity);
                SUMMA_TEST_ASSERT_NULL(frame->bindings.value);
            }
        }
        summa_scheme_value_free(&closure);
    }
}

void test_environment_call_frame_grows_for_an_internal_define() {
    SUMMA_TEST_SCOPED_VALUE(
        SummaSchemeEnvironment, global, summa_scheme_environment_make_global(), summa_scheme_environment_free) {
        SummaSchemeValue closure = {};
        /* A frame sized to one parameter, then given a second binding by the
         * body. Growing from an exact fit is the case right-sizing has to keep
         * working. */
        const SummaSchemeError err =
            evaluate_source(global, "((lambda (a) (define b 7) (lambda () (+ a b))) 1)", &closure);
        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            const SummaSchemeEnvironment frame = closure.value.procedure.closure;
            SUMMA_TEST_ASSERT_NOT_NULL(frame);
            if (frame) {
                SUMMA_TEST_ASSERT_EQ(2u, frame->bindings.length);
                SUMMA_TEST_ASSERT(frame->bindings.capacity >= 2u);
                SummaSchemeBinding out = {};
                SUMMA_TEST_ASSERT(!summa_scheme_environment_get(frame, symbol_of("b"), &out).had);
                SUMMA_TEST_ASSERT_EQ(7, out.value.value.integer.value);
            }
        }
        summa_scheme_value_free(&closure);
    }
}

void test_environment_let_frame_is_sized_to_its_clauses() {
    SUMMA_TEST_SCOPED_VALUE(
        SummaSchemeEnvironment, global, summa_scheme_environment_make_global(), summa_scheme_environment_free) {
        SummaSchemeValue       closure = {};
        const SummaSchemeError err     = evaluate_source(global, "(let ((a 1) (b 2) (c 3)) (lambda () a))", &closure);
        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            const SummaSchemeEnvironment frame = closure.value.procedure.closure;
            SUMMA_TEST_ASSERT_NOT_NULL(frame);
            if (frame) {
                SUMMA_TEST_ASSERT_EQ(3u, frame->bindings.length);
                SUMMA_TEST_ASSERT_EQ(3u, frame->bindings.capacity);
            }
        }
        summa_scheme_value_free(&closure);
    }
}

void test_environment_wide_call_frame_costs_no_doubling() {
    SUMMA_TEST_SCOPED_VALUE(
        SummaSchemeEnvironment, global, summa_scheme_environment_make_global(), summa_scheme_environment_free) {
        SummaSchemeValue closure = {};
        /* Ten parameters: past the old default of eight, where the frame used
         * to pay a realloc on top of the over-allocation. */
        const SummaSchemeError err = evaluate_source(
            global, "((lambda (a b c d e f g h i j) (lambda () (+ a j))) 1 2 3 4 5 6 7 8 9 10)", &closure);
        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            const SummaSchemeEnvironment frame = closure.value.procedure.closure;
            SUMMA_TEST_ASSERT_NOT_NULL(frame);
            if (frame) {
                SUMMA_TEST_ASSERT_EQ(10u, frame->bindings.length);
                SUMMA_TEST_ASSERT_EQ(10u, frame->bindings.capacity);
            }
        }
        summa_scheme_value_free(&closure);
    }
}

#pragma endregion Binding storage is sized to the frame

int main(int argc, char** argv) {
    summa_test_begin("scheme.environment", argc, argv);
    SUMMA_TEST_RUN(test_environment_make_empty_starts_unbound);
    SUMMA_TEST_RUN(test_environment_survives_the_block_that_made_it);
    SUMMA_TEST_RUN(test_environment_set_then_get);
    SUMMA_TEST_RUN(test_environment_set_existing_name_rebinds_in_place);
    SUMMA_TEST_RUN(test_environment_rebinding_releases_the_previous_value);
    SUMMA_TEST_RUN(test_environment_distinct_names_both_bound);
    SUMMA_TEST_RUN(test_environment_get_unbound_name_errors);
    SUMMA_TEST_RUN(test_environment_get_falls_through_to_parent);
    SUMMA_TEST_RUN(test_environment_child_binding_shadows_parent);
    SUMMA_TEST_RUN(test_environment_freeing_child_leaves_parent_alive);
    SUMMA_TEST_RUN(test_environment_make_empty_allocates_no_binding_storage);
    SUMMA_TEST_RUN(test_environment_make_with_capacity_reserves_exactly);
    SUMMA_TEST_RUN(test_environment_grows_past_the_capacity_it_was_given);
    SUMMA_TEST_RUN(test_environment_grows_from_zero_capacity);
    SUMMA_TEST_RUN(test_environment_reserve_only_grows);
    SUMMA_TEST_RUN(test_environment_global_frame_is_sized_to_the_builtins);
    SUMMA_TEST_RUN(test_environment_call_frame_is_sized_to_the_arity);
    SUMMA_TEST_RUN(test_environment_nullary_call_frame_holds_no_binding_storage);
    SUMMA_TEST_RUN(test_environment_call_frame_grows_for_an_internal_define);
    SUMMA_TEST_RUN(test_environment_let_frame_is_sized_to_its_clauses);
    SUMMA_TEST_RUN(test_environment_wide_call_frame_costs_no_doubling);
    return summa_test_end();
}
