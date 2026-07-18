#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

void test_scheme_read() {
    SummaSchemeValue value;
    SummaSchemeError error = summa_scheme_read("(define x 1)", &value);
    // TODO: Make this not fail
    SUMMA_TEST_ASSERT(error.had);
    SUMMA_TEST_ASSERT_EQ_STR("summa_scheme_read - NOT IMPLEMENTED", error.message);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.read", argc, argv);

    SUMMA_TEST_RUN(test_scheme_read);

    return summa_test_end();
}
