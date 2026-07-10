#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#include <summa/identity.h>
#include <summa/identity.h> /* verify include guard */

void test_identity_int(void) {
    for (int i = -100; i < 100; ++i)
        SUMMA_TEST_ASSERT_EQ_INT(i, summa_identity(i));
}

void test_identity_float(void) { SUMMA_TEST_ASSERT(3.14f == summa_identity(3.14f)); }

int main(void) {
    summa_test_begin("identity");
    SUMMA_TEST_RUN(test_identity_int);
    SUMMA_TEST_RUN(test_identity_float);
    return summa_test_end();
}
