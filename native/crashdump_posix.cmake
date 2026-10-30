if(UNIX)
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_common.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_posix_state.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_posix_io.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/phobos_stdio.cmake")
    add_library(nijigenerate_crashdump_posix STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_paths.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_signal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_install.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_notify.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_message.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_report.cpp")
    target_include_directories(nijigenerate_crashdump_posix PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_crashdump_posix PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_crashdump_posix PRIVATE
        nijigenerate_crashdump_common nijigenerate_crashdump_posix_state nijigenerate_crashdump_posix_io
        nijigenerate_tinyfiledialogs)
    target_link_libraries(nijigenerate_crashdump_posix PUBLIC
        nijigenerate_runtime_core nijigenerate_core nijigenerate_phobos_stdio)
endif()
