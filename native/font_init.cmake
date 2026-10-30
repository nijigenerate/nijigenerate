if(TARGET nijigenerate_font AND TARGET nijigenerate_resources AND TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_font PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_init.cpp")
    target_link_libraries(nijigenerate_font PUBLIC nijigenerate_core nijigenerate_runtime_core
        PRIVATE nijigenerate_resources)
    add_dependencies(nijigenerate_font nijigenerate_generate_resources)
endif()
