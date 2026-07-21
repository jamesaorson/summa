#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

static void free_env(SummaSchemeEnvironment env) {
    summa_binding_list_free(env->bindings);
}

/* The environment is a compound literal with automatic storage duration, so it
 * has to be built in the scope that uses it -- which is where the macro's
 * loop-init clause puts it -- rather than returned from a helper. */
#define SCOPED_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeEnvironment, var, summa_scheme_environment_make_empty(), free_env)

void test_scheme_read() {
    SCOPED_ENV(env) {
        SummaSchemeValue value;
        SummaSchemeError error = summa_scheme_read(env, "(define x 1)", &value);
        // TODO: Make this not fail
        SUMMA_TEST_ASSERT(error.had);
        SUMMA_TEST_ASSERT_EQ_STR("summa_scheme_read - NOT IMPLEMENTED", error.message);
    }
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.read", argc, argv);

    SUMMA_TEST_RUN(test_scheme_read);

    return summa_test_end();
}
