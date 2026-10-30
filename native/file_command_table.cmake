if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_table.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_helpers.cpp")
    if(NIJIGENERATE_IMGUI)
        target_sources(nijigenerate_runtime_core PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_realtime.cpp")
        target_link_libraries(nijigenerate_runtime_core PRIVATE imgui_sdl3)
    endif()
endif()
