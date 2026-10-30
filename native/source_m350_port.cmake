# Existing M350 original only. This registers no generated case or test execution.
if(TARGET nijigenerate_autorig_framework AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_phobos_stdio)
    add_library(nijigenerate_source_m350 STATIC
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_json_storage.cpp")
    target_sources(nijigenerate_source_m350 PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_json_storage.hpp")
    target_include_directories(nijigenerate_source_m350 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_compile_features(nijigenerate_source_m350 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m350 PUBLIC
        nijigenerate_autorig_framework nijigenerate_runtime_core nijigenerate_phobos_stdio)
    if(MSVC)
        target_compile_options(nijigenerate_source_m350 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m350 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_autorig_json_storage_source
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_json_storage_main.cpp")
    target_link_libraries(nijigenerate_autorig_json_storage_source PRIVATE nijigenerate_source_m350)
    if(MINGW)
        target_link_options(nijigenerate_autorig_json_storage_source PRIVATE -municode)
    endif()
endif()
