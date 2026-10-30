# Exact authored source. Its known MIR typing errors remain; no replacement case.
if(TARGET nijigenerate_core AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_phobos_stdio
        AND NOT TARGET nijigenerate_source_distance_transform_unittest)
    add_library(nijigenerate_source_distance_transform_unittest STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/distance_transform_unittest.cpp")
    target_sources(nijigenerate_source_distance_transform_unittest PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/distance_transform_unittest.hpp")
    target_compile_features(nijigenerate_source_distance_transform_unittest PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_distance_transform_unittest PUBLIC
        nijigenerate_core nijigenerate_runtime_core nijigenerate_phobos_stdio)
    if(MSVC)
        target_compile_options(nijigenerate_source_distance_transform_unittest PRIVATE /fp:strict /utf-8)
    else()
        target_compile_options(nijigenerate_source_distance_transform_unittest PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
