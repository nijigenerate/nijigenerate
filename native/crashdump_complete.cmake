if(WIN32 AND NOT TARGET nijigenerate_crashdump_windows)
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_windows.cmake")
endif()
if(WIN32 AND TARGET nijigenerate_crashdump_windows)
    target_sources(nijigenerate_crashdump_windows PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_handler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_report.cpp")
    target_link_libraries(nijigenerate_crashdump_windows PUBLIC nijigenerate_runtime_core
        nijigenerate_crashdump_common nijigenerate_crashdump_thread)
    if(MSVC)
        target_compile_options(nijigenerate_crashdump_windows PRIVATE /fp:strict)
    endif()
endif()
