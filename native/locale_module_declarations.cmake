if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    add_library(nijigenerate_locale_module_declarations OBJECT tests/locale_module_declarations.cpp)
    target_link_libraries(nijigenerate_locale_module_declarations PRIVATE nijigenerate_runtime_core)
endif()
