# Existing Source M339 cases only; building/running is an explicit separate step.
option(NIJIGENERATE_BUILD_SOURCE_M339 "Build the existing Source numeric serialization unittest" OFF)
if(NIJIGENERATE_BUILD_SOURCE_M339)
    if(NOT TARGET nicxlive::nicxlive OR NOT TARGET nijigenerate_phobos_stdio)
        message(FATAL_ERROR "M339 requires the actual SDK and canonical Phobos stdout provider")
    endif()
    add_library(nijigenerate_source_m339 STATIC
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/numeric_serialization.cpp")
    target_sources(nijigenerate_source_m339 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/numeric_serialization.hpp")
    target_include_directories(nijigenerate_source_m339 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_include_directories(nijigenerate_source_m339 PRIVATE "${nijigenerate_common_sdk_root}")
    target_link_libraries(nijigenerate_source_m339 PUBLIC nicxlive::nicxlive nijigenerate_phobos_stdio)
    target_compile_features(nijigenerate_source_m339 PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_source_m339 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m339 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_m339_unittest
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/numeric_serialization_main.cpp")
    target_link_libraries(nijigenerate_source_m339_unittest PRIVATE nijigenerate_source_m339)
endif()
