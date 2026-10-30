if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_common.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_thread.cmake")
    add_library(nijigenerate_crashdump_windows STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_state.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_io.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_helpers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_paths.cpp")
    target_include_directories(nijigenerate_crashdump_windows PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_crashdump_windows PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_crashdump_windows PRIVATE
        kernel32 shell32 user32 dbghelp nijigenerate_runtime_core nijigenerate_crashdump_common)
endif()
