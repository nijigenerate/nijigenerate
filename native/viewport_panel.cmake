if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/viewport.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET viewport_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/viewport.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels
        nijigenerate_button nijigenerate_label nijigenerate_tooltip nijigenerate_dialog_queue
        nijigenerate_viewport nijigenerate_asyncderivedupdatedetails nijigenerate_asyncderivedupdateoverlay
        nijigenerate_viewport_control_commands)
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/viewport.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/viewport.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
