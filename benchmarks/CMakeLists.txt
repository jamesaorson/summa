# Benchmarks are built like examples and run like neither: nothing here is
# registered with CTest, so `make test` never pays for them, and a benchmark
# that stopped compiling still fails the build.
set(ALL_BENCHMARKS scheme)

message("Benchmarks to be built: ${ALL_BENCHMARKS}")

foreach(benchmark ${ALL_BENCHMARKS})
    add_executable(benchmarks.${benchmark})
    set_property(TARGET benchmarks.${benchmark} PROPERTY C_STANDARD 23)
    target_sources(benchmarks.${benchmark} PRIVATE ${benchmark}/main.c)
    target_compile_options(
        benchmarks.${benchmark}
        PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
    target_link_libraries(
        benchmarks.${benchmark}
        PRIVATE summa::summa
    )
endforeach()
