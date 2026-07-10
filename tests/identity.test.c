#include <summa/identity.h>
#include <summa/identity.h> /* verify include guard */

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_identity_int(void) {
    for (int i = -100; i < 100; ++i) {
        TEST_ASSERT_EQUAL_INT(i, summa_identity(i));
    }
}

void test_identity_float(void) { TEST_ASSERT_EQUAL_FLOAT(3.14f, summa_identity(3.14f)); }

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_identity_int);
    RUN_TEST(test_identity_float);
    return UNITY_END();
}
