if(TARGET nijigenerate_autorig_storage AND TARGET nijigenerate_autorig_framework
    AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_phobos_stdio
    AND NOT TARGET nijigenerate_source_m354)
    find_package(Threads REQUIRED)
    add_library(nijigenerate_source_m354 STATIC
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_state_storage.cpp")
    target_include_directories(nijigenerate_source_m354 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_sources(nijigenerate_source_m354 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_state_storage.hpp")
    target_compile_features(nijigenerate_source_m354 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m354 PUBLIC nijigenerate_autorig_storage
        nijigenerate_autorig_framework nijigenerate_runtime_core nijigenerate_phobos_stdio Threads::Threads)
    if(MSVC)
        target_compile_options(nijigenerate_source_m354 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m354 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_autorig_state_storage EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_state_storage_main.cpp")
    target_link_libraries(nijigenerate_source_autorig_state_storage PRIVATE nijigenerate_source_m354)
    if(MINGW)
        target_link_options(nijigenerate_source_autorig_state_storage PRIVATE -municode)
    endif()
endif()
