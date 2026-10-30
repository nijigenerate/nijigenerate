if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_flipconfig AND TARGET nijigenerate_transform_complete)
    add_library(nijigenerate_parameter_base STATIC src/nijigenerate/commands/parameter/base.cpp)
    target_link_libraries(nijigenerate_parameter_base PUBLIC nijigenerate_runtime_core
        nijigenerate_flipconfig nijigenerate_transform_complete PRIVATE nijigenerate_native_options)
    target_sources(nijigenerate_parameter_base PUBLIC FILE_SET parameter_base_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/base.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_parameter_base PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_parameter_base PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
