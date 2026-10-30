if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_viewport_mesh
        AND TARGET nijigenerate_transform_declarations)
    add_library(nijigenerate_transform_complete STATIC src/nijigenerate/utils/transform.cpp)
    target_link_libraries(nijigenerate_transform_complete PUBLIC nijigenerate_transform_declarations)
    target_sources(nijigenerate_transform_complete PRIVATE FILE_SET transform_internal_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/utils/transform_internal.hpp")
    target_link_libraries(nijigenerate_transform_complete PUBLIC
        nijigenerate_runtime_core nijigenerate_viewport_mesh PRIVATE nijigenerate_native_options)
    if(MSVC)
        target_compile_options(nijigenerate_transform_complete PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_transform_complete PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
