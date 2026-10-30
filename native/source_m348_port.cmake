if(TARGET nijigenerate_autorig_framework AND TARGET nijigenerate_autorig_workflow
        AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_phobos_stdio
        AND TARGET SDL3::SDL3 AND NOT TARGET nijigenerate_source_m348)
    find_package(Threads REQUIRED)
    add_library(nijigenerate_source_m348 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_framework.cpp")
    target_sources(nijigenerate_source_m348 PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_framework.hpp")
    target_include_directories(nijigenerate_source_m348 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_compile_features(nijigenerate_source_m348 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m348 PUBLIC nijigenerate_autorig_framework
        nijigenerate_autorig_workflow nijigenerate_runtime_core nijigenerate_phobos_stdio Threads::Threads)
    if(MSVC)
        # The authored standalone original uses enabled D assertions.
        target_compile_options(nijigenerate_source_m348 PRIVATE /UNDEBUG /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m348 PRIVATE -UNDEBUG -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_autorig_framework EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_framework_main.cpp")
    target_link_libraries(nijigenerate_source_autorig_framework PRIVATE nijigenerate_source_m348 SDL3::SDL3)
endif()
