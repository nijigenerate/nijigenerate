if(NOT TARGET nijigenerate_crashdump_common)
    add_library(nijigenerate_crashdump_common STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_common.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_common_paths.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_timestamp.cpp")
    target_include_directories(nijigenerate_crashdump_common PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_crashdump_common PUBLIC cxx_std_20)
    target_sources(nijigenerate_crashdump_common PUBLIC FILE_SET crash_path_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_build_path.hpp")
    target_link_libraries(nijigenerate_crashdump_common PUBLIC nijigenerate_runtime_core)
    target_sources(nijigenerate_crashdump_common PUBLIC FILE_SET relative_path_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_relative_path.hpp")
    if(WIN32)
        target_link_libraries(nijigenerate_crashdump_common PRIVATE kernel32)
    endif()
endif()
