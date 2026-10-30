if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_phobos_stdio
        AND NOT TARGET nijigenerate_source_m125)
    add_library(nijigenerate_source_m125 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/selector_package_unittest.cpp")
    target_sources(nijigenerate_source_m125 PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/selector_package_unittest.hpp")
    target_include_directories(nijigenerate_source_m125 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_compile_features(nijigenerate_source_m125 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m125 PUBLIC nijigenerate_runtime_core nijigenerate_phobos_stdio)
    if(MSVC)
        target_compile_options(nijigenerate_source_m125 PRIVATE /fp:strict /utf-8)
    else()
        target_compile_options(nijigenerate_source_m125 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_selector_unittest EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/selector_package_unittest_main.cpp")
    target_link_libraries(nijigenerate_source_selector_unittest PRIVATE nijigenerate_source_m125)
endif()
