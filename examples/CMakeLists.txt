set(ALL_EXAMPLES hello)

message("Examples to be built: ${ALL_EXAMPLES}")

foreach(example ${ALL_EXAMPLES})
    add_executable(examples.${example})
    set_property(TARGET examples.${example} PROPERTY C_STANDARD 23)
    target_sources(examples.${example} PRIVATE ${example}/main.c)
    target_link_libraries(
        examples.${example}
        PRIVATE summa::summa
    )
endforeach()
