#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_PARQUET_IMPLEMENTATION
#include <summa/parquet/parquet.h>

void test_parquet_load() {
    SUMMA_TEST_TODO("test_parquet_load: NOT IMPLEMENTED");
}

int main(int argc, char** argv) {
    summa_test_begin("parquet", argc, argv);
    SUMMA_TEST_RUN(test_parquet_load);
    return summa_test_end();
}
