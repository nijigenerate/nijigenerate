option(NIJIGENERATE_BUILD_SOURCE_M355 "Build the existing Source deformation interpolation cases" OFF)
if(NIJIGENERATE_BUILD_SOURCE_M355)
    if(NOT TARGET nijigenerate_runtime_core OR NOT TARGET nicxlive::nicxlive)
        message(FATAL_ERROR "M355 requires actual SDK deformation and App format/monotonic-time providers")
    endif()
    add_library(nijigenerate_source_m355 STATIC
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/deformation_interpolation.cpp")
    target_sources(nijigenerate_source_m355 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/deformation_interpolation.hpp")
    target_include_directories(nijigenerate_source_m355 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_link_libraries(nijigenerate_source_m355 PUBLIC nijigenerate_runtime_core nicxlive::nicxlive)
    target_compile_features(nijigenerate_source_m355 PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_source_m355 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m355 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_m355_cases
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/deformation_interpolation_main.cpp")
    target_link_libraries(nijigenerate_source_m355_cases PRIVATE nijigenerate_source_m355)
endif()
