if(WIN32 AND TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_crashdump_thread)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_thread.cpp")
    add_library(nijigenerate_crashdump_thread INTERFACE)
    target_link_libraries(nijigenerate_crashdump_thread INTERFACE nijigenerate_runtime_core)
endif()
