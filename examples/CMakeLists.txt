set(ALL_EXAMPLES hello)

message("Examples to be built: ${ALL_EXAMPLES}")

foreach(example ${ALL_EXAMPLES})
    add_executable(examples.${example})
    set_property(TARGET examples.${example} PROPERTY CXX_STANDARD 20)
    target_sources(examples.${example} PRIVATE ${example}/main.cpp)
    target_link_libraries(
        examples.${example}
        PRIVATE jamesaorson::template_cpp
    )
endforeach()
