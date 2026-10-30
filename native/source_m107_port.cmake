# Port the authored original; do not turn its known failure into a passing case.
if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_source_m107)
    add_library(nijigenerate_source_m107 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/cv_image_unittest.cpp")
    target_sources(nijigenerate_source_m107 PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/cv_image_unittest.hpp")
    target_include_directories(nijigenerate_source_m107 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_compile_features(nijigenerate_source_m107 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m107 PUBLIC nijigenerate_core)
    # Match the assertion-enabled original -unittest profile, including MIR checks.
    if(MSVC)
        target_compile_options(nijigenerate_source_m107 PRIVATE /UNDEBUG /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m107 PRIVATE -UNDEBUG -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_image_unittest EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/cv_image_unittest_main.cpp")
    target_link_libraries(nijigenerate_source_image_unittest PRIVATE nijigenerate_source_m107)
endif()
