# Exact authored unittest; known Source MIR typing failures remain preserved.
if(TARGET nijigenerate_cv AND TARGET nijigenerate_runtime_core
        AND NOT TARGET nijigenerate_source_contours_unittest)
    add_library(nijigenerate_source_contours_unittest STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/contours_unittest.cpp")
    target_compile_features(nijigenerate_source_contours_unittest PUBLIC cxx_std_20)
    target_sources(nijigenerate_source_contours_unittest PUBLIC
        FILE_SET source_m105_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/contours_unittest.hpp")
    target_link_libraries(nijigenerate_source_contours_unittest PUBLIC
        nijigenerate_cv nijigenerate_runtime_core)
    if(MSVC)
        target_compile_options(nijigenerate_source_contours_unittest PRIVATE /fp:strict /utf-8)
    else()
        target_compile_options(nijigenerate_source_contours_unittest PRIVATE
            -ffp-contract=off -fno-fast-math)
    endif()
endif()
