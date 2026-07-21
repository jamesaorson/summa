#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#define SCOPED_ENV(var)      \
    SUMMA_TEST_SCOPED_VALUE( \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_empty(), summa_scheme_environment_free)

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

#define SCOPED_STRING(var, init) SUMMA_TEST_SCOPED_VALUE(SummaString, var, init, summa_string_free)

/* set() takes ownership of both halves of the binding, so the name is made
 * fresh at every call site rather than shared. */
static void bind_integer(SummaSchemeEnvironment env, const char* name, int64_t value) {
    summa_scheme_environment_set(env,
                                 summa_scheme_binding_make(summa_string_make(name), summa_make_scheme_integer(value)));
}

static SummaSchemeSymbol symbol_of(SummaString name) {
    return (SummaSchemeSymbol){.value = name};
}

void test_environment_make_empty_starts_unbound() {
    SCOPED_ENV(env) {
        SUMMA_TEST_ASSERT_NOT_NULL(env);
        SUMMA_TEST_ASSERT_NOT_NULL(env->bindings);
        SUMMA_TEST_ASSERT_EQ(0u, env->bindings->length);
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
    SUMMA_TEST_ASSERT_EQ(1u, env->bindings->length);
    summa_scheme_environment_free(env);
}

void test_environment_set_then_get() {
    SCOPED_ENV(env)
    SCOPED_STRING(lookup, summa_string_make("x")) {
        bind_integer(env, "x", 42);
        SUMMA_TEST_ASSERT_EQ(1u, env->bindings->length);

        SummaSchemeBinding out;
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of(lookup), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(42, out.value.value.integer.value);
    }
}

void test_environment_set_existing_name_rebinds_in_place() {
    SCOPED_ENV(env)
    SCOPED_STRING(lookup, summa_string_make("x")) {
        bind_integer(env, "x", 1);
        bind_integer(env, "x", 2);

        /* Rebinding replaces rather than appends. The by-value copy this used
         * to make meant the second set updated a temporary and the environment
         * kept reporting 1. */
        SUMMA_TEST_ASSERT_EQ(1u, env->bindings->length);

        SummaSchemeBinding out;
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of(lookup), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(2, out.value.value.integer.value);
    }
}

void test_environment_rebinding_releases_the_previous_value() {
    SCOPED_ENV(env)
    SCOPED_STRING(lookup, summa_string_make("s")) {
        /* The displaced string value has to be freed by set(), not orphaned --
         * a leak the leaks(1) run over this binary would catch. */
        summa_scheme_environment_set(
            env, summa_scheme_binding_make(summa_string_make("s"), summa_make_scheme_string("first")));
        summa_scheme_environment_set(
            env, summa_scheme_binding_make(summa_string_make("s"), summa_make_scheme_string("second")));

        SUMMA_TEST_ASSERT_EQ(1u, env->bindings->length);

        SummaSchemeBinding out;
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of(lookup), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ_STR("second", out.value.value.string.value->value);
    }
}

void test_environment_distinct_names_both_bound() {
    SCOPED_ENV(env)
    SCOPED_STRING(lookup_x, summa_string_make("x"))
    SCOPED_STRING(lookup_y, summa_string_make("y")) {
        bind_integer(env, "x", 1);
        bind_integer(env, "y", 2);
        SUMMA_TEST_ASSERT_EQ(2u, env->bindings->length);

        SummaSchemeBinding out;
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(env, symbol_of(lookup_x), &out).had);
        SUMMA_TEST_ASSERT_EQ(1, out.value.value.integer.value);
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(env, symbol_of(lookup_y), &out).had);
        SUMMA_TEST_ASSERT_EQ(2, out.value.value.integer.value);
    }
}

void test_environment_get_unbound_name_errors() {
    SCOPED_ENV(env)
    SCOPED_STRING(lookup, summa_string_make("nope")) {
        SummaSchemeBinding out;
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of(lookup), &out);
        SUMMA_TEST_ASSERT(error.had);
        SUMMA_TEST_ASSERT_EQ_STR("Unbound variable: nope", error.message);
    }
}

void test_environment_get_falls_through_to_parent() {
    SCOPED_ENV(parent)
    SCOPED_STRING(lookup, summa_string_make("outer")) {
        bind_integer(parent, "outer", 7);

        SummaSchemeEnvironment child = summa_scheme_environment_make(parent);
        SUMMA_TEST_ASSERT_EQ(parent, child->parent);

        SummaSchemeBinding out;
        SummaSchemeError   error = summa_scheme_environment_get(child, symbol_of(lookup), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(7, out.value.value.integer.value);

        summa_scheme_environment_free(child);
    }
}

void test_environment_child_binding_shadows_parent() {
    SCOPED_ENV(parent)
    SCOPED_STRING(lookup, summa_string_make("x")) {
        bind_integer(parent, "x", 1);

        SummaSchemeEnvironment child = summa_scheme_environment_make(parent);
        bind_integer(child, "x", 2);

        SummaSchemeBinding out;
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(child, symbol_of(lookup), &out).had);
        SUMMA_TEST_ASSERT_EQ(2, out.value.value.integer.value);
        /* The parent's own binding is untouched by the shadow. */
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(parent, symbol_of(lookup), &out).had);
        SUMMA_TEST_ASSERT_EQ(1, out.value.value.integer.value);

        summa_scheme_environment_free(child);
    }
}

void test_environment_freeing_child_leaves_parent_alive() {
    SCOPED_ENV(parent)
    SCOPED_STRING(lookup, summa_string_make("x")) {
        bind_integer(parent, "x", 1);

        SummaSchemeEnvironment child = summa_scheme_environment_make(parent);
        summa_scheme_environment_free(child);

        /* parent is borrowed by the child, never owned, so it is still usable
         * here. The reverse -- a parent outliving its children -- is the
         * constraint callers currently have to honour by hand. */
        SummaSchemeBinding out;
        SUMMA_TEST_ASSERT(!summa_scheme_environment_get(parent, symbol_of(lookup), &out).had);
        SUMMA_TEST_ASSERT_EQ(1, out.value.value.integer.value);
    }
}

void test_environment_global_binds_plus() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_STRING(lookup, summa_string_make("+")) {
        SUMMA_TEST_ASSERT_EQ(1u, env->bindings->length);

        SummaSchemeBinding out;
        SummaSchemeError   error = summa_scheme_environment_get(env, symbol_of(lookup), &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeProcedureType, out.value.type);
        SUMMA_TEST_ASSERT_EQ_STR("+", out.value.value.procedure.name->value);
        /* The binding's name and the procedure's name must be two allocations,
         * not one shared handle -- otherwise freeing the environment releases
         * the same string twice. */
        SUMMA_TEST_ASSERT_NEQ(out.name, out.value.value.procedure.name);
    }
}

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
    SUMMA_TEST_RUN(test_environment_global_binds_plus);
    return summa_test_end();
}
