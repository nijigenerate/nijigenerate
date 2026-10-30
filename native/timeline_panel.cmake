if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels AND TARGET nijigenerate_timeline)
    target_sources(nijigenerate_runtime_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/timeline.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels nijigenerate_timeline
        nijigenerate_toolbar nijigenerate_tooltip nijigenerate_dummy)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET timeline_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/timeline.hpp")
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/timeline.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/timeline.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
