#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

void test_scheme_evaluate() {
    SummaSchemeValue in = summa_make_scheme_boolean(true);
    SummaSchemeValue out;
    SummaSchemeError error = summa_scheme_evaluate(in, &out);
    // TODO: Make this not fail
    SUMMA_TEST_ASSERT(error.had);
    SUMMA_TEST_ASSERT_EQ_STR("summa_scheme_evaluate - NOT IMPLEMENTED", error.message);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.evaluate", argc, argv);

    SUMMA_TEST_RUN(test_scheme_evaluate);

    return summa_test_end();
}
